#include "disk.h"

// Buffer temporal para la BIOS (en 0x70000, alineado a 64 KB)
#define BIOS_BUFFER 0x70000

// Funciones en ensamblador
extern "C" void disk_set_lba(unsigned int lba);
extern "C" void disk_set_boot_drive(unsigned char drive);
extern "C" int  disk_read_sector_asm();
extern "C" int  disk_get_bios_result();

static unsigned char boot_drive = 0;

void disk_init(unsigned char drive) {
    boot_drive = drive;
    disk_set_boot_drive(drive);
}

int disk_read_sector(unsigned int lba, unsigned char *buffer) {
    // Pasar el LBA al driver
    disk_set_lba(lba);

    // Llamar a la rutina que vuelve a modo real
    int result = disk_read_sector_asm();

    if (result != 0) {
        return result;   // error
    }

    // Copiar del buffer de la BIOS al buffer del llamante
    volatile unsigned char *src = (volatile unsigned char*)BIOS_BUFFER;
    for (int i = 0; i < 512; i++) {
        buffer[i] = src[i];
    }

    return 0;
}
