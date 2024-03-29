/*****************************************************************************
* File Name: file_system.h
*
* Description:
*  This file contains the function prototypes and constants used in
*  the file_system.c.
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

#if !defined(FILE_SYSTEM_H)
#define FILE_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>
#include "cy_device_headers.h"

/* Size of the LOG file. Must be power of 2 and at least 512 bytes */
#define CONFIG_LOG_FILE_SIZE                    2048u
#define FILE_SYSTEM_ROOT_DIRECTORIES_ENTRIES    16
#define FILE_SYSTEM_SECTORS_PER_FAT             4
#define FILE_SYSTEM_SECTOR_SIZE                 512
#define FILE_SYSTEM_SECTORS_PER_CLUSTER         (CONFIG_LOG_FILE_SIZE/FILE_SYSTEM_SECTOR_SIZE)
#define FILE_SYSTEM_DATASIZE                    (FILE_SYSTEM_SECTOR_SIZE*FILE_SYSTEM_SECTORS_PER_CLUSTER)
#define FILE_SYSTEM_SECTORS_PER_FS              (7+FILE_SYSTEM_SECTORS_PER_CLUSTER)
#define FILE_SYSTEM_SECTORS_PER_TRACK           0x12
#define FILE_SYSTEM_SIGNATURE                   0xAA55
#define FILE_SYSTEM_JUMP_BOOTSTRAP_SIZE         3
#define FILE_SYSTEM_OEM_SIZE                    8
#define FILE_SYSTEM_BOOTSTRAP_SIZE              480
#define FILE_SYSTEM_FILENAME_SIZE               11
#define FILE_SYSTEM_RESERVED_SIZE               10

/*******************************************************************************
* Constants
*******************************************************************************/
/* Jump Bootstrap Address */
#define FILE_SYSTEM_JUMP_BOOTSTRAP_VAL0     0xEB
#define FILE_SYSTEM_JUMP_BOOTSTRAP_VAL1     0x3C
#define FILE_SYSTEM_JUMP_BOOTSTRAP_VAL2     0x90

/* Media Type
 *  0xF8 - standard value for non-removable disks
 *  0xF0 - non partitioned removable disks */
#define FILE_SYSTEM_MEDIA_TYPE              0xF8

/* FAT Area */
#define FILE_SYSTEM_FAT_AREA_VAL0           0xF8
#define FILE_SYSTEM_FAT_AREA_VAL1           0xFF
#define FILE_SYSTEM_FAT_AREA_VAL2           0xFF
#define FILE_SYSTEM_FAT_AREA_VAL3           0xFF
#define FILE_SYSTEM_FAT_AREA_VAL4           0x0F

/* Directory Attribute
 * 0x01: Read Only
 * 0x02: Hidden
 * 0x03: System
 * 0x08: Volume Label
 * 0x10: Directory
 * 0x20: Archive
 * 0x0F: Long File Name */
#define FILE_SYSTEM_ATTRIBUTE_DRIVE         0x28
#define FILE_SYSTEM_ATTRIBUTE_LOG_FILE      0x01

/* Time
 * Bit 15-11: Hours in range from 0 to 23
 * Bit 10-5:  Minutes in range from 0 to 59
 * Bit 4-0:   2 second count in range of form 0 to 29 (0-58 seconds) */
#define FILE_SYSTEM_TIME                    0x7B6E

/* Date
 * Bit 15-9: Count of years from 1980 in range of from 0 to 127 (1980-2107)
 * Bit 8-5:  Month of year in range from 1 to 12
 * Bit 4-0:  Day of month in range from 1 to 31 */
#define FILE_SYSTEM_DATE                    0x42B6

/* Cluster Address for the Log file */
#define FILE_SYSTEM_LOG_FILE_ADDRESS        2

/* File name and Extension define */
#define FILE_NAME_DEF(NAME,EXT) (NAME EXT)

/* Initial Log message in the LOG file. If not used, comment the line below.
 * Limited to the size of the LOG file */
#define CONFIG_LOG_MESSAGE          "PMG1 MSC Device Content:"

typedef struct
{
    uint8_t  jump_bootstrap[FILE_SYSTEM_JUMP_BOOTSTRAP_SIZE];
    char     oem[FILE_SYSTEM_OEM_SIZE];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_copies;
    uint16_t root_entries;
    uint16_t sectors_per_fs;
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint16_t hidden_sectors;
    uint8_t  bootstrap[FILE_SYSTEM_BOOTSTRAP_SIZE];
    uint16_t signature;
} __PACKED boot_sector_t;

typedef struct
{
    char     filename[FILE_SYSTEM_FILENAME_SIZE];
    uint8_t  attribute;
    uint8_t  reserved[FILE_SYSTEM_RESERVED_SIZE];
    uint16_t time;
    uint16_t date;
    uint16_t cluster;
    uint32_t filesize;
} __PACKED directory_entry_t;

typedef struct
{
    uint8_t content[FILE_SYSTEM_SECTOR_SIZE*FILE_SYSTEM_SECTORS_PER_CLUSTER];
} cluster_t;

typedef struct
{
    boot_sector_t boot;
    uint8_t fat_area[FILE_SYSTEM_SECTOR_SIZE*FILE_SYSTEM_SECTORS_PER_FAT];
    directory_entry_t entries[FILE_SYSTEM_ROOT_DIRECTORIES_ENTRIES];
    cluster_t clusters[1];
} __PACKED fat_fs_t;

void file_system_init(uint8_t **mem);
bool file_system_write(const char *buf, uint32_t len);
void file_system_erase_all(void);

#endif /* FILE_SYSTEM_H */
