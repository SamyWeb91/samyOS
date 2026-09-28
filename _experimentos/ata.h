#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void ata_init();
int ata_read_sector(unsigned int lba, unsigned char *buffer);

#ifdef __cplusplus
}
#endif
