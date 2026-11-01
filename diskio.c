/*
 * Copyright (C) 2026 Samuel Tamayo Infante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ff.h"
#include "diskio.h"
#include "ata.h"

#define SECTOR_SIZE   512
#define TOTAL_SECTORS 128

DSTATUS disk_status (BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;
    return 0;
}

DSTATUS disk_initialize (BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;
    ata_init();
    return 0;
}

DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0) return RES_PARERR;
    if (!buff || count == 0) return RES_PARERR;

    for (UINT i = 0; i < count; i++) {
        if (ata_read_sector(sector + i, buff + (i * SECTOR_SIZE)) != 0) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

#if FF_FS_READONLY == 0
DRESULT disk_write (BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    (void)pdrv; (void)buff; (void)sector; (void)count;
    return RES_ERROR;
}
#endif

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv != 0) return RES_PARERR;
    switch (cmd) {
        case CTRL_SYNC: return RES_OK;
        case GET_SECTOR_COUNT: *(LBA_t*)buff = TOTAL_SECTORS; return RES_OK;
        case GET_SECTOR_SIZE: *(WORD*)buff = SECTOR_SIZE; return RES_OK;
        case GET_BLOCK_SIZE: *(DWORD*)buff = 1; return RES_OK;
        default: return RES_PARERR;
    }
}

#if FF_FS_NORTC == 0
DWORD get_fattime (void) {
    return ((DWORD)(2024 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16);
}
#endif
