#pragma once

// Inicializa el driver (guarda el numero de unidad del USB)
void disk_init(unsigned char boot_drive);

// Lee un sector del USB a un buffer
// Devuelve 0 si OK, != 0 si error
int disk_read_sector(unsigned int lba, unsigned char *buffer);
