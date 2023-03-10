/*****************************************************************************
* File Name: file_system.c
*
* Description:
*  This file provides the source code to emulate a file system. It only
*  contains a single file for read-only operations from the OS side and
*  write only operations from the firmware side.
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

#include "file_system.h"
#include "cy_utils.h"
#include <string.h>

/*******************************************************************************
* Global Variables
*******************************************************************************/
#ifdef CONFIG_LOG_MESSAGE
    const unsigned char initial_log_message[] = CONFIG_LOG_MESSAGE;
#endif

fat_fs_t file_system;
uint32_t file_system_write_count = 0;

/*******************************************************************************
* Function Name: file_system_init
********************************************************************************
* Summary:
*  Initialize the file system. It creates only one file in the system and its
*  content is placed in the internal SRAM.
*
* Parameters:
*  emulated_memory [out]: pointer to the memory. Note that the memory is
*  allocated internally.
*
*******************************************************************************/
void file_system_init(uint8_t **emulated_memory)
{
    uint8_t *content;
    *emulated_memory = (uint8_t *) &file_system;

    file_system_erase_all();

    /* Fill up the boot sector */
    file_system.boot.jump_bootstrap[0] = FILE_SYSTEM_JUMP_BOOTSTRAP_VAL0;
    file_system.boot.jump_bootstrap[1] = FILE_SYSTEM_JUMP_BOOTSTRAP_VAL1;
    file_system.boot.jump_bootstrap[2] = FILE_SYSTEM_JUMP_BOOTSTRAP_VAL2;
    file_system.boot.bytes_per_sector = FILE_SYSTEM_SECTOR_SIZE;
    file_system.boot.sectors_per_cluster = FILE_SYSTEM_SECTORS_PER_CLUSTER;
    file_system.boot.reserved_sectors = 1;
    file_system.boot.fat_copies = 1;
    file_system.boot.root_entries = FILE_SYSTEM_ROOT_DIRECTORIES_ENTRIES;
    file_system.boot.sectors_per_fs = FILE_SYSTEM_SECTORS_PER_FS;
    file_system.boot.media_type = FILE_SYSTEM_MEDIA_TYPE;
    file_system.boot.sectors_per_fat = FILE_SYSTEM_SECTORS_PER_FAT;
    file_system.boot.sectors_per_track = FILE_SYSTEM_SECTORS_PER_TRACK;
    file_system.boot.heads = 1;
    file_system.boot.signature = FILE_SYSTEM_SIGNATURE;

    /* Update FAT area */
    file_system.fat_area[0] = FILE_SYSTEM_FAT_AREA_VAL0;
    file_system.fat_area[1] = FILE_SYSTEM_FAT_AREA_VAL1;
    file_system.fat_area[2] = FILE_SYSTEM_FAT_AREA_VAL2;
    file_system.fat_area[3] = FILE_SYSTEM_FAT_AREA_VAL3;
    file_system.fat_area[4] = FILE_SYSTEM_FAT_AREA_VAL4;

    /* In the first entry, set the Mass Storage Device Name */
    memcpy(file_system.entries[0].filename, "PMG1 Drive", FILE_SYSTEM_FILENAME_SIZE);
    file_system.entries[0].attribute = FILE_SYSTEM_ATTRIBUTE_DRIVE;

    /* In the second entry, set up the LOG file */
    memcpy(file_system.entries[1].filename,FILE_NAME_DEF("LOG     ","TXT"), 11);
    file_system.entries[1].attribute = FILE_SYSTEM_ATTRIBUTE_LOG_FILE;
    file_system.entries[1].reserved[1] = 0x0E;
    file_system.entries[1].reserved[2] = 0xEB;
    file_system.entries[1].reserved[3] = 0x7C;
    file_system.entries[1].reserved[4] = 0xB6;
    file_system.entries[1].reserved[5] = 0x42;
    file_system.entries[1].reserved[6] = 0xB6;
    file_system.entries[1].reserved[7] = 0x42;
    file_system.entries[1].time = FILE_SYSTEM_TIME;
    file_system.entries[1].date = FILE_SYSTEM_DATE;
    file_system.entries[1].cluster = FILE_SYSTEM_LOG_FILE_ADDRESS;

    /* Point to the beginning of the content */
    content = file_system.clusters[0].content;

#ifdef CONFIG_LOG_MESSAGE
    /* Point to the beginning of the content */
    content = file_system.clusters[0].content;

    /* Write the initial log message and update file size */
    memcpy(content, initial_log_message, sizeof(initial_log_message)-1);
    file_system.entries[1].filesize = sizeof(initial_log_message) - 1;
    file_system_write_count = sizeof(initial_log_message) - 1;
#endif

}

/*******************************************************************************
* Function Name: file_system_write
********************************************************************************
* Summary:
*  Writes data to the internal file located in SRAM.
*
* Parameters:
*  buf: pointer to the data
*  len: number of bytes to write
*
* Return:
*  True if successful. False if overflow the size of the file.
*
*******************************************************************************/
bool file_system_write(const char *buf, uint32_t len)
{
    bool status = false;
    uint8_t msgIndex = 0;
    uint8_t *content;

    if ((file_system.entries[1].filesize + len) < (FILE_SYSTEM_DATASIZE))
    {
        content = (file_system.clusters[0].content);

        /* Status message. */
        for(msgIndex = 0; msgIndex < len; msgIndex++, file_system_write_count++)
        {
            content[file_system_write_count] = buf[msgIndex];
        }

        /* Set the new file size */
        file_system.entries[1].filesize = file_system_write_count;

        status = true;
    }

    return status;
}

/*******************************************************************************
* Function Name: file_system_erase_all
********************************************************************************
* Summary:
*  Erase the file content.
*
*******************************************************************************/
void file_system_erase_all(void)
{
    /* Fill the emulated memory with spaces. */
    memset(file_system.clusters, 0, FILE_SYSTEM_DATASIZE);

    file_system.entries[1].filesize = 0;
    file_system_write_count = 0;
}
