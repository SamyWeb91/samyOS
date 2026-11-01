// Copyright (C) 2026 Samuel Tamayo Infante
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "ata.h"
#include "io.h"

#define ATA_DATA        0x1F0
#define ATA_SECCOUNT    0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_COMMAND     0x1F7
#define ATA_STATUS      0x1F7

#define ATA_CMD_READ    0x20

#define ATA_BSY         0x80
#define ATA_DRQ         0x08
#define ATA_ERR         0x01

static void ata_wait_bsy() {
    while (inb(ATA_STATUS) & ATA_BSY);
}

static void ata_wait_drq() {
    while (!(inb(ATA_STATUS) & ATA_DRQ));
}

extern "C" void ata_init() {
    ata_wait_bsy();
}

extern "C" int ata_read_sector(unsigned int lba, unsigned char *buffer) {
    ata_wait_bsy();

    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW,  (unsigned char)(lba & 0xFF));
    outb(ATA_LBA_MID,  (unsigned char)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, (unsigned char)((lba >> 16) & 0xFF));

    outb(ATA_COMMAND, ATA_CMD_READ);

    ata_wait_drq();

    unsigned short *buf = (unsigned short*)buffer;
    for (int i = 0; i < 256; i++) {
        buf[i] = inw(ATA_DATA);
    }

    unsigned char status = inb(ATA_STATUS);
    if (status & ATA_ERR) return 1;

    return 0;
}
