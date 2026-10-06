#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ahci_init();
int ahci_read_sector(uint64_t lba, void *buffer);
int ahci_write_sector(uint64_t lba, const void *buffer);
uint64_t ahci_get_sector_count();
int ahci_is_ready();

#ifdef __cplusplus
}
#endif
