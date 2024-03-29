/*****************************************************************************
* File Name: usb_comm.c
*
* Description:
*  This file provides the source code to implement the USB Mass Storage
*  class requests.
*
* Note:
*
*******************************************************************************
* Copyright 2022-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#include "usb_comm.h"
#include "usb_scsi.h"

#include "cy_sysint.h"
#include "cycfg.h"
#include "cycfg_usbdev.h"

/*******************************************************************************
* Local USB Callbacks
*******************************************************************************/
static cy_en_usb_dev_status_t usb_msc_request_received (cy_stc_usb_dev_control_transfer_t *transfer,
                                                         void *classContext,
                                                         cy_stc_usb_dev_context_t *devContext);

static cy_en_usb_dev_status_t usb_msc_request_completed(cy_stc_usb_dev_control_transfer_t *transfer,
                                                         void *classContext,
                                                         cy_stc_usb_dev_context_t *devContext);

static cy_en_usb_dev_status_t usb_msc_in_requests(cy_stc_usb_dev_msc_context_t *context);
static cy_en_usb_dev_status_t usb_msc_out_requests(cy_stc_usb_dev_msc_context_t *context);

/***************************************************************************
* USB Interrupt Handlers
***************************************************************************/
static void usb_high_isr(void);
static void usb_medium_isr(void);
static void usb_low_isr(void);
void usb_timer_isr(void);

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* USB Interrupt Configuration */
const cy_stc_sysint_t usb_high_interrupt_cfg =
{
    .intrSrc = (IRQn_Type) usb_interrupt_hi_IRQn,
    .intrPriority = 0U,
};
const cy_stc_sysint_t usb_medium_interrupt_cfg =
{
    .intrSrc = (IRQn_Type) usb_interrupt_med_IRQn,
    .intrPriority = 1U,
};
const cy_stc_sysint_t usb_low_interrupt_cfg =
{
    .intrSrc = (IRQn_Type) usb_interrupt_lo_IRQn,
    .intrPriority = 2U,
};

/* USB Timer Interrupt Configuration */
const cy_stc_sysint_t usb_timer_interrupt_cfg = {
        .intrSrc = CYBSP_USB_TIMER_IRQ,              /* Source of interrupt signal */
        .intrPriority = USB_TIMER_INTR_PRIORITY      /* Interrupt priority */
};

/* USBDEV context variables */
cy_stc_usbfs_dev_drv_context_t  usb_drvContext;
cy_stc_usb_dev_context_t        usb_devContext;
cy_stc_usb_dev_msc_context_t    usb_mscContext;

/* USB MSC specific variables */
uint8_t msc_lun = 0;
uint8_t msc_reset = 0;
bool    msc_continue_out = true;
bool    msc_continue_in  = false;

volatile bool usb_suspended = false;
volatile uint32_t usb_idle_counter = 0;

uint8_t *usb_fs = NULL;

extern uint8_t forceOS;
extern uint8_t statusFileTimer;

/*******************************************************************************
* Function Name: usb_comm_init
********************************************************************************
* Summary:
*   Initializes the USBFS hardware block and its interrupts.
*
*******************************************************************************/
void usb_comm_init(void)
{
    /* Init the USB Block */
    Cy_USB_Dev_Init(CYBSP_USBDEV_HW,
                    &CYBSP_USBDEV_config,
                    &usb_drvContext,
                    &usb_devices[USB_COMM_DEVICE_ID],
                    &usb_devConfig,
                    &usb_devContext);

    /* Init the Mass Storage Device Class */
    Cy_USB_Dev_Msc_Init(NULL,
                        &usb_mscContext,
                        &usb_devContext);

    /* Register Mass Storage Callbacks */
    Cy_USB_Dev_Msc_RegisterUserCallback(usb_msc_request_received, usb_msc_request_completed, &usb_mscContext);

    /* Initialize the USB interrupts */
    Cy_SysInt_Init(&usb_high_interrupt_cfg,   &usb_high_isr);
    Cy_SysInt_Init(&usb_medium_interrupt_cfg, &usb_medium_isr);
    Cy_SysInt_Init(&usb_low_interrupt_cfg,    &usb_low_isr);

    /* Initialize the USB timer interrupt */
    Cy_SysInt_Init(&usb_timer_interrupt_cfg, &usb_timer_isr);

    /* Init the timer to detect USB activity */
    Cy_TCPWM_Counter_Init(CYBSP_USB_TIMER_HW, CYBSP_USB_TIMER_NUM, &CYBSP_USB_TIMER_config);
    Cy_TCPWM_Counter_Enable(CYBSP_USB_TIMER_HW, CYBSP_USB_TIMER_NUM);
    Cy_TCPWM_SetInterrupt(CYBSP_USB_TIMER_HW, CYBSP_USB_TIMER_NUM, CY_TCPWM_INT_ON_TC);
}

/*******************************************************************************
* Function Name: usb_comm_connect
********************************************************************************
* Summary:
*   Starts USB enumeration.
*
*******************************************************************************/
void usb_comm_connect(void)
{
    /* Enable the USB interrupts */
    NVIC_EnableIRQ(usb_high_interrupt_cfg.intrSrc);
    NVIC_EnableIRQ(usb_medium_interrupt_cfg.intrSrc);
    NVIC_EnableIRQ(usb_low_interrupt_cfg.intrSrc);

    /* Make device appear on the bus. This function call is blocking,
       it waits till the device enumerates */
    Cy_USB_Dev_Connect(true, CY_USB_DEV_WAIT_FOREVER, &usb_devContext);

    /* Enable the USB timer interrupt */
    NVIC_EnableIRQ(usb_timer_interrupt_cfg.intrSrc);

    /* Start the internal timer to check for activity.
     * Trigger a software start on the counter instance. This is required when
     * no other hardware input signal is connected to the component to act as
     * a trigger source.
     */
    Cy_TCPWM_TriggerStart(CYBSP_USB_TIMER_HW, CYBSP_USB_TIMER_MASK);

    /* Enable the OUT Endpoint */
    Cy_USB_Dev_StartReadEp(MSC_OUT_ENDPOINT, &usb_devContext);
}

/*******************************************************************************
* Function Name: usb_comm_link_fs
********************************************************************************
* Summary:
*   Link the file system to the USB.
*
* Parameters:
*   fs: pointer to the file system
*
*******************************************************************************/
void usb_comm_link_fs(uint8_t *fs)
{
    usb_fs = fs;
}

/*******************************************************************************
* Function Name: usb_comm_is_ready
********************************************************************************
* Summary:
*   Verifies if the USB is enumerated.
*
*******************************************************************************/
bool usb_comm_is_ready(void)
{
    return (Cy_USB_Dev_GetConfiguration(&usb_devContext));
}

void usb_comm_refresh(void)
{
    forceOS = true;
    statusFileTimer = 0;
}

/*******************************************************************************
* Function Name: usb_comm_process
********************************************************************************
* Summary:
*   Process any pending requests from USB.
*
*******************************************************************************/
void usb_comm_process(void)
{
    /* Check if any activity */
    if (usb_suspended || (!usb_comm_is_ready()))
    {
        usb_suspended = false;

        /* Disable timer handler */
        Cy_TCPWM_ClearInterrupt(CYBSP_USB_TIMER_HW, CYBSP_USB_TIMER_NUM, CY_TCPWM_INT_ON_TC);

        /* Reconnect the USB block */
        Cy_USB_Dev_Disconnect(&usb_devContext);
        Cy_USB_Dev_Connect(true, CY_USB_DEV_WAIT_FOREVER, &usb_devContext);
        Cy_USB_Dev_StartReadEp(MSC_OUT_ENDPOINT, &usb_devContext);

        /* Re-enable timer handler */
        Cy_TCPWM_SetInterrupt(CYBSP_USB_TIMER_HW, CYBSP_USB_TIMER_NUM, CY_TCPWM_INT_ON_TC);
    }

    if (msc_continue_in || (Cy_USBFS_Dev_Drv_GetEndpointState(CYBSP_USBDEV_HW, MSC_IN_ENDPOINT, &usb_drvContext) == CY_USB_DEV_EP_PENDING))
    {
        msc_continue_in = false;
        usb_msc_in_requests(&usb_mscContext);
    }

    if (msc_continue_out || (Cy_USBFS_Dev_Drv_GetEndpointState(CYBSP_USBDEV_HW, MSC_OUT_ENDPOINT, &usb_drvContext) == CY_USB_DEV_EP_PENDING))
    {
        msc_continue_out = false;
        usb_msc_out_requests(&usb_mscContext);
    }
}

/*******************************************************************************
* Function Name: usb_msc_in_requests
********************************************************************************
* Summary:
*   Interprets the SCSI IN commands and implements a non-blocking state machine
*   to service the requests.
*
* Parameters:
*   context: USB MSC context
*
* Return:
*   Success if the routine handling SCSI is successful.
*
*******************************************************************************/
static cy_en_usb_dev_status_t usb_msc_in_requests(cy_stc_usb_dev_msc_context_t *context)
{
    cy_en_usb_dev_status_t status = CY_USB_DEV_REQUEST_NOT_HANDLED;

    switch (context->state)
    {
        /* Process and Send data to host (Data-In) */
        case CY_USB_DEV_MSC_DATA_IN:
            switch (context->cmd_block.cmd[0])
            {
                case CY_USB_DEV_MSC_SCSI_READ10:
                    status = usb_scsi_read_sense_10(context, usb_fs);
                    msc_continue_in = true;
                    break;

                case CY_USB_DEV_MSC_SCSI_REQUEST_SENSE:
                    status = usb_scsi_request_sense(context);
                    msc_continue_in = true;
                    break;

                case CY_USB_DEV_MSC_SCSI_INQUIRY:
                    status = usb_scsi_inquiry(context);
                    msc_continue_in = true;
                    break;

                case CY_USB_DEV_MSC_SCSI_MODE_SENSE6:
                    status = usb_scsi_mode_sense_6(context);
                    msc_continue_in = true;
                    break;

                case CY_USB_DEV_MSC_SCSI_MODE_SENSE10:
                    status = usb_scsi_mode_sense_10(context);
                    msc_continue_in = true;
                    break;

                case CY_USB_DEV_MSC_SCSI_READ_CAPACITY:
                    status = usb_scsi_read_capacity(context);
                    msc_continue_in = true;
                    break;

                case CY_USB_DEV_MSC_SCSI_READ_FORMAT_CAPACITIES:
                    status = usb_scsi_read_format_capacities(context);
                    msc_continue_in = true;
                    break;

                case CY_USB_DEV_MSC_SCSI_FORMAT_UNIT:
                    status = usb_scsi_format_unit(context);
                    msc_continue_in = true;
                    break;

                case CY_USB_DEV_MSC_SCSI_MODE_SELECT6:
                    status = usb_scsi_mode_select_6(context);
                    msc_continue_in = true;
                    break;

                case CY_USB_DEV_MSC_SCSI_MODE_SELECT10:
                    status = usb_scsi_mode_select_10(context);
                    msc_continue_in = true;
                    break;

                default:
                    break;
            }

            /* Send the data to host only if the status is success */
            if (status == CY_USB_DEV_SUCCESS)
            {
                /* Send data to host */
                Cy_USB_Dev_WriteEpBlocking(MSC_IN_ENDPOINT, context->in_buffer, context->packet_in_size, CY_USB_DEV_WAIT_FOREVER, &usb_devContext);
                context->cmd_status.data_residue -= context->packet_in_size;

                /* If command is Read10, the update the  start location to
                 * process next packet as the data can be sent in multiple
                 *  packets */
                if (context->cmd_block.cmd[0] == CY_USB_DEV_MSC_SCSI_READ10)
                {
                    context->start_location += context->packet_in_size;
                    context->bytes_to_transfer -= context->packet_in_size;

                    /* Send CSW once Data stage is completed */
                    if (context->bytes_to_transfer == 0)
                    {
                        context->state = CY_USB_DEV_MSC_STATUS_TRANSPORT;
                        msc_continue_in = true;
                    }

                    if (context->state != CY_USB_DEV_MSC_DATA_IN)
                    {
                        context->cmd_status.status = CY_USB_DEV_SUCCESS;
                    }
                }
            }
            context->cmd_status.status = status;
            break;

        /* Process the MSC commands which does not data stage */
        case CY_USB_DEV_MSC_NO_DATA:
            switch (context->cmd_block.cmd[0])
            {
                case CY_USB_DEV_MSC_SCSI_TEST_UNIT_READY:
                    status = usb_scsi_test_unit_ready();
                    break;

                case CY_USB_DEV_MSC_SCSI_MEDIA_REMOVAL:
                    status = usb_scsi_prevent_media_removal(context->cmd_block.cmd[4]);
                    break;

                case CY_USB_DEV_MSC_SCSI_START_STOP_UNIT:
                    status = usb_scsi_start_stop_unit(context->cmd_block.cmd[4]);
                    break;

                default:
                    break;
            }

            /* Send the CSW after processing the command */
            context->state = CY_USB_DEV_MSC_STATUS_TRANSPORT;
            msc_continue_in = true;
            context->cmd_status.status = status;
            break;

        case CY_USB_DEV_MSC_STALL_IN_ENDPOINT:
            Cy_USBFS_Dev_Drv_StallEndpoint(CYBSP_USBDEV_HW, MSC_IN_ENDPOINT, &usb_drvContext);
            context->state = CY_USB_DEV_MSC_READY_STATE;
            msc_continue_out = true;
            break;

        /* Send Command Status Wrapper (CSW) */
        case CY_USB_DEV_MSC_STATUS_TRANSPORT:
            Cy_USB_Dev_WriteEpBlocking(MSC_IN_ENDPOINT, (const uint8_t *) &context->cmd_status, CY_USB_DEV_MSC_CMD_STATUS_SIZE, CY_USB_DEV_WAIT_FOREVER, &usb_devContext);
            context->state = CY_USB_DEV_MSC_READY_STATE;
            msc_continue_out = true;
            break;

        default:
            break;
    }

    return status;
}

/*******************************************************************************
* Function Name: is_command_block_wrapper_valid
********************************************************************************
* Summary:
*   Check for validity of the Command Block Wrapper (CBW).
*
* Parameters:
*   context: USB MSC context
*
* Return:
*   Success if valid.
* 
*******************************************************************************/
uint8_t is_command_block_wrapper_valid(cy_stc_usb_dev_msc_context_t *context)
{
    cy_en_usb_dev_status_t validStatus = CY_USB_DEV_BAD_PARAM;
    uint8_t index = 0;

    for(index = 0; index < CY_USB_DEV_MSC_CMD_BLOCK_SIZE; index++)
    {
        /* Copy all contents from EP buffer to structure. May replace with DMA,
         * need to validate if efficiency can be increased in that way. */
        *((uint8_t *)&context->cmd_block + index) = context->out_buffer[index];
    }

    if(context->cmd_block.signature == MSC_CBW_SIGNATURE)
    {
        if(context->cmd_block.lun <= 0)
        {
            if((context->cmd_block.length > 0) && (context->cmd_block.length <= CY_USB_DEV_MSC_CMD_SIZE))
            {
                /* Validate all conditions of section 6.6.1 of MSC spec. */
                validStatus = CY_USB_DEV_SUCCESS;
            }
        }
    }
    return(validStatus);
}

/*******************************************************************************
* Function Name: usb_msc_out_requests
********************************************************************************
* Summary:
*   Interprets the SCSI OUT commands and implements a non-blocking state machine
*   to service the requests
*
* Parameters:
*   context: USB MSC context
*
* Return
*   Success if the routine handling SCSI is successful.
*
*******************************************************************************/
static cy_en_usb_dev_status_t usb_msc_out_requests(cy_stc_usb_dev_msc_context_t *context)
{
    uint8_t epCount = 0;
    uint32_t tempVar = 0;
    uint32_t actCount;

    switch (context->state)
    {
        case CY_USB_DEV_MSC_READY_STATE:
            /* Number of bytes received now should be exactly 1 as per spec */
            if (Cy_USBFS_Dev_Drv_GetEndpointAckState(CYBSP_USBDEV_HW, MSC_OUT_ENDPOINT) != 0)
            {
                /* Read the Command Block Wrapper from the USB endpoint */
                epCount = Cy_USBFS_Dev_Drv_GetEndpointCount(CYBSP_USBDEV_HW, MSC_OUT_ENDPOINT);
                if (epCount == CY_USB_DEV_MSC_CMD_BLOCK_SIZE)
                {
                    Cy_USB_Dev_ReadEpBlocking(MSC_OUT_ENDPOINT, context->out_buffer, epCount, &actCount, CY_USB_DEV_WAIT_FOREVER, &usb_devContext);
                    context->state = CY_USB_DEV_MSC_COMMAND_TRANSPORT;
                }
                else
                {
                    context->state = CY_USB_DEV_MSC_STALL_OUT_ENDPOINT;
                }
            }
            msc_continue_out = true;
            break;

        /* Receive and process Command Block Wrapper (CBW) */
        case CY_USB_DEV_MSC_COMMAND_TRANSPORT:
            if (is_command_block_wrapper_valid(context) == CY_USB_DEV_SUCCESS)
            {
                /* Check if there is data stage */
                if (context->cmd_block.data_transfer_length != 0)
                {
                    /* Check for the data flow direction as per section 5.1 of MSC spec. */
                    if ((context->cmd_block.flags & USB_COMM_CBW_FLAG_DIR_IN) == USB_COMM_CBW_FLAG_DIR_IN)
                    {
                        context->state = CY_USB_DEV_MSC_DATA_IN;
                        msc_continue_in = true;

                        /* Update the data buffer controls if the command is Read10 */
                        if (context->cmd_block.cmd[0] == CY_USB_DEV_MSC_SCSI_READ10)
                        {
                            tempVar = ((context->cmd_block.cmd[2] << 24) |
                                       (context->cmd_block.cmd[3] << 16) |
                                       (context->cmd_block.cmd[4] << 8)  |
                                       (context->cmd_block.cmd[5]));
                            context->start_location = tempVar * MSC_BLOCKSIZE;

                            tempVar = ((context->cmd_block.cmd[7] << 8)  |
                                       (context->cmd_block.cmd[8]));
                            context->bytes_to_transfer = tempVar * MSC_BLOCKSIZE;

                            if (context->cmd_block.data_transfer_length != context->bytes_to_transfer)
                            {
                                context->cmd_status.status = USB_COMM_CBS_PHASE_ERROR;
                                context->state = CY_USB_DEV_MSC_STALL_OUT_ENDPOINT;
                            }
                        }
                    }
                    else
                    {
                        context->state = CY_USB_DEV_MSC_DATA_OUT;

                        /* Update the data buffer controls if the command is Write10 or Verify10 */
                        if ((context->cmd_block.cmd[0] == CY_USB_DEV_MSC_SCSI_WRITE10) ||
                            (context->cmd_block.cmd[0] == CY_USB_DEV_MSC_SCSI_VERIFY10))
                        {
                            tempVar = ((context->cmd_block.cmd[2] << 24) |
                                       (context->cmd_block.cmd[3] << 16) |
                                       (context->cmd_block.cmd[4] << 8)  |
                                       (context->cmd_block.cmd[5]));
                            context->start_location = tempVar * MSC_BLOCKSIZE;

                            tempVar = ((context->cmd_block.cmd[7] << 8)  |
                                       (context->cmd_block.cmd[8]));
                            context->bytes_to_transfer = tempVar * MSC_BLOCKSIZE;

                            if (context->cmd_block.data_transfer_length != context->bytes_to_transfer)
                            {
                                context->cmd_status.status = USB_COMM_CBS_PHASE_ERROR;
                                context->state = CY_USB_DEV_MSC_STALL_OUT_ENDPOINT;
                            }
                        }
                    }
                }
                else
                {
                    context->state = CY_USB_DEV_MSC_NO_DATA;
                    msc_continue_in = true;
                }

                context->cmd_status.tag = context->cmd_block.tag;
                context->cmd_status.data_residue = context->cmd_block.data_transfer_length;

                /* Send the CSW or continue to the Data stage */
                if (context->state == CY_USB_DEV_MSC_STATUS_TRANSPORT)
                {
                    msc_continue_in = true;
                }
                else
                {
                    msc_continue_out = true;
                }
                /* Enable the OUT Endpoint */
                Cy_USB_Dev_StartReadEp(MSC_OUT_ENDPOINT, &usb_devContext);
            }
            else
            {
                context->state = CY_USB_DEV_MSC_WAIT_FOR_RESET;
                msc_continue_out = true;
            }
            break;

        /* Receive and process data from host (Data-Out) */
        case CY_USB_DEV_MSC_DATA_OUT:
            switch (context->cmd_block.cmd[0])
            {
                case CY_USB_DEV_MSC_SCSI_WRITE10:
                    /* Check if the Endpoint sends Ack to the last USB transaction */
                    if (Cy_USBFS_Dev_Drv_GetEndpointAckState(CYBSP_USBDEV_HW, MSC_OUT_ENDPOINT) != 0)
                    {
                        if (context->toggle_out == 0)
                        {
                            context->packet_out_size = Cy_USBFS_Dev_Drv_GetEndpointCount(CYBSP_USBDEV_HW, MSC_OUT_ENDPOINT);
                            Cy_USB_Dev_ReadEpBlocking(MSC_OUT_ENDPOINT, context->out_buffer, context->packet_out_size, &actCount, USB_COMM_TIMEOUT, &usb_devContext);

                            if (context->packet_out_size != actCount)
                            {
                                context->state = CY_USB_DEV_MSC_READY_STATE;
                                msc_continue_out = true;

                                /* Enable the OUT Endpoint */
                                Cy_USB_Dev_StartReadEp(MSC_OUT_ENDPOINT, &usb_devContext);
                                break;
                            }
                        }

                        usb_scsi_write_10(context, usb_fs);

                        /* Enable the OUT Endpoint */
                        Cy_USB_Dev_StartReadEp(MSC_OUT_ENDPOINT, &usb_devContext);
                    }

                    /* Send the CSW or continue the Data-Out stage */
                    if (context->state == CY_USB_DEV_MSC_STATUS_TRANSPORT)
                    {
                        msc_continue_in = true;
                    }
                    else
                    {
                        msc_continue_out = true;
                    }
                    break;

                case CY_USB_DEV_MSC_SCSI_VERIFY10:
                    /* Check if the Endpoint sends Ack to the last USB transaction */
                    if (Cy_USBFS_Dev_Drv_GetEndpointAckState(CYBSP_USBDEV_HW, MSC_OUT_ENDPOINT) != 0)
                    {
                        context->packet_out_size = Cy_USBFS_Dev_Drv_GetEndpointCount(CYBSP_USBDEV_HW, MSC_OUT_ENDPOINT);
                        Cy_USB_Dev_ReadEpBlocking(MSC_OUT_ENDPOINT, context->out_buffer, context->packet_out_size, &actCount, USB_COMM_TIMEOUT, &usb_devContext);

                        usb_scsi_verify_10(context);

                        /* Enable the OUT Endpoint */
                        Cy_USB_Dev_StartReadEp(MSC_OUT_ENDPOINT, &usb_devContext);
                    }

                    /* Send the CSW or continue the Data-Out stage */
                    if (context->state == CY_USB_DEV_MSC_STATUS_TRANSPORT)
                    {
                        msc_continue_in = true;
                    }
                    else
                    {
                        msc_continue_out = true;
                    }
                    break;

                default:
                    break;
            }
            break;

        case CY_USB_DEV_MSC_STALL_OUT_ENDPOINT:
            Cy_USBFS_Dev_Drv_StallEndpoint(CYBSP_USBDEV_HW, MSC_OUT_ENDPOINT, &usb_drvContext);
            context->state = CY_USB_DEV_MSC_READY_STATE;
            msc_continue_out = true;
            break;

        case CY_USB_DEV_MSC_WAIT_FOR_RESET:
            Cy_USBFS_Dev_Drv_StallEndpoint(CYBSP_USBDEV_HW, MSC_OUT_ENDPOINT, &usb_drvContext);
            Cy_USBFS_Dev_Drv_StallEndpoint(CYBSP_USBDEV_HW, MSC_IN_ENDPOINT, &usb_drvContext);
            msc_continue_out = true;
            break;
        default:
            break;
    }
    return CY_USB_DEV_SUCCESS;
}

/*******************************************************************************
* Function Name: usb_msc_request_received
********************************************************************************
* Summary:
*   Callback implementation for the MSC Request Received.
*
* Parameters:
*   transfer: contain information about the transfer
*   classContext: pointer to the class context
*   devContext: USB device context
*
* Return:
*   Success if supported request received.
*
*******************************************************************************/
cy_en_usb_dev_status_t usb_msc_request_received(cy_stc_usb_dev_control_transfer_t *transfer,
                                                 void *classContext,
                                                 cy_stc_usb_dev_context_t *devContext)
{
    cy_en_usb_dev_status_t retStatus = CY_USB_DEV_REQUEST_NOT_HANDLED;

    if (transfer->setup.bmRequestType.type == CY_USB_DEV_CLASS_TYPE)
    {
        switch (transfer->setup.bRequest)
        {
            case CY_USB_DEV_MSC_GET_MAX_LUN:
                transfer->remaining = 0x01;
                transfer->ptr = &msc_lun;
                retStatus = CY_USB_DEV_SUCCESS;
                break;
            case CY_USB_DEV_MSC_RESET:
                transfer->notify = true;
                transfer->ptr = &msc_reset;
                transfer->remaining = 0x01;
                retStatus = CY_USB_DEV_SUCCESS;
                break;
            default:
                break;
        }
    }

    return retStatus;
}


/*******************************************************************************
* Function Name: usb_msc_request_completed
********************************************************************************
* Summary:
*   Callback implementation for MSC Msc Request Completed. Not used in this
*   example.
*
* Parameters:
*   transfer: contain information about the transfer
*   classContext: pointer to the class context
*   devContext: USB device context
*
* Return:
*   Not handled. 
*
*******************************************************************************/
static cy_en_usb_dev_status_t usb_msc_request_completed(cy_stc_usb_dev_control_transfer_t *transfer,
                                                         void *classContext,
                                                         cy_stc_usb_dev_context_t *devContext)
{
    cy_en_usb_dev_status_t retStatus = CY_USB_DEV_REQUEST_NOT_HANDLED;

    return retStatus;
}

/***************************************************************************
* Function Name: usb_timer_handler
********************************************************************************
* Summary:
*   Internal interrupt handler for the USB.
*
***************************************************************************/
void usb_timer_isr(void)
{
    /* Clear the terminal count interrupt */
    Cy_TCPWM_ClearInterrupt(CYBSP_USB_TIMER_HW, CYBSP_USB_TIMER_NUM, CY_TCPWM_INT_ON_TC );

    usb_scsi_serve_timeout();

    if (0u != Cy_USBFS_Dev_Drv_CheckActivity(CYBSP_USBDEV_HW))
    {
        usb_idle_counter = 0;
    }
    else
    {
        /* Check for suspend condition on USB */
        if (usb_idle_counter < USB_COMM_SUSPEND_COUNT)
        {
            /* Counter idle time before detect suspend condition */
            usb_idle_counter++;
        }
        else
        {
            usb_suspended = true;
        }
    }
}

/***************************************************************************
* Function Name: usb_high_isr
********************************************************************************
* Summary:
*  This function process the high priority USB interrupts.
*
***************************************************************************/
static void usb_high_isr(void)
{
    /* Call interrupt processing */
    Cy_USBFS_Dev_Drv_Interrupt(CYBSP_USBDEV_HW,
                               Cy_USBFS_Dev_Drv_GetInterruptCauseHi(CYBSP_USBDEV_HW),
                               &usb_drvContext);
}


/***************************************************************************
* Function Name: usb_medium_isr
********************************************************************************
* Summary:
*  This function process the medium priority USB interrupts.
*
***************************************************************************/
static void usb_medium_isr(void)
{
    /* Call interrupt processing */
    Cy_USBFS_Dev_Drv_Interrupt(CYBSP_USBDEV_HW,
                               Cy_USBFS_Dev_Drv_GetInterruptCauseMed(CYBSP_USBDEV_HW),
                               &usb_drvContext);
}


/***************************************************************************
* Function Name: usb_low_isr
********************************************************************************
* Summary:
*  This function process the low priority USB interrupts.
*
**************************************************************************/
static void usb_low_isr(void)
{
    /* Call interrupt processing */
    Cy_USBFS_Dev_Drv_Interrupt(CYBSP_USBDEV_HW,
                               Cy_USBFS_Dev_Drv_GetInterruptCauseLo(CYBSP_USBDEV_HW),
                               &usb_drvContext);
}
