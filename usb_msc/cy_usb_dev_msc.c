/***************************************************************************//**
* \file cy_usb_dev_msc.c
* \version 0.1
*
* Provides Mass Storage class-specific API implementation.
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

#include "cy_usb_dev_msc.h"

#if (defined(CY_IP_MXUSBFS) || defined(CY_IP_M0S8USBDSS))

/*******************************************************************************
* Function Name: Cy_USB_Dev_Msc_Init
****************************************************************************//**
*
* Initializes the Mass Storage class.
* This function must be called to enable USB Device Mass Storage functionality.
*
* \param config
* Pass NULL as an argument (left for future purposes).
*
* \param context
* The pointer to the context structure \ref cy_stc_usb_dev_msc_context_t
* allocated by the user. The structure is used during the MSC Class operation
* for internal configuration and data retention. The user must not modify
* anything in this structure.
*
* \param devContext
* The pointer to the USB Device context structure \ref cy_stc_usb_dev_context_t.
*
* \return
* Status code of the function execution \ref cy_en_usb_dev_status_t.
*
*******************************************************************************/
cy_en_usb_dev_status_t Cy_USB_Dev_Msc_Init(void const *config,
                                             cy_stc_usb_dev_msc_context_t      *context,
                                             cy_stc_usb_dev_context_t          *devContext)
{
    /* Suppress a compiler warning about unused variables */
    (void) config;

    if ((NULL == context) || (NULL == devContext))
    {
        return CY_USB_DEV_BAD_PARAM;
    }

    context->state = CY_USB_DEV_MSC_READY_STATE;
    context->toggle_in = 0;
    context->toggle_out = 0;
    context->cmd_status.signature = MSC_CSW_SIGNATURE;

    /* Store device context */
    context->devContext = devContext;

    return Cy_USB_Dev_RegisterClass(&context->classItem, &context->classObj, context, devContext);
}

#endif /* (defined(CY_IP_MXUSBFS) || defined(CY_IP_M0S8USBDSS)) */


/* [] END OF FILE */

