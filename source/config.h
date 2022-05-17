/*****************************************************************************
* File Name: config.h
*
* Description:
*  Application configuration constants.
*
* Note:
*
*******************************************************************************
* Copyright 2022, Cypress Semiconductor Corporation (an Infineon company) or
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

#ifndef CONFIG_H_
#define CONFIG_H_

/*******************************************************************************
* Configuration Constants
********************************************************************************/
/* Size of the LOG file. Must be power of 2 and at least 512 bytes */
#define CONFIG_LOG_FILE_SIZE        2048u

/* Name of the LOG file. Must have 8 characters and no special characters.
 * If the name has less than 8 characters, fill it up with spaces */
#define CONFIG_LOG_FILE_NAME        "LOG     "

/* Extension of the LOG file. Must have 3 characters */
#define CONFIG_LOG_FILE_EXTENSION   "TXT"

/* Name of the Mass Storage Drive. Must have 8 characters and no special characters.
 * If the name has less than 8 characters, fill it up with spaces */
#define CONFIG_DRIVE_NAME           "PMG1 Drive"

/* Initial Log message in the LOG file. If not used, comment the line below. 
 * Limited to the size of the LOG file */
#define CONFIG_LOG_MESSAGE          "PMG1 MSC Device Content:"

#endif /* CONFIG_H_ */
