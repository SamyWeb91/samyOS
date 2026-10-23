// fat32.h
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =====================================================================
 * FAT32 - Driver de lectura (solo lectura)
 * ===================================================================== */

#define FAT32_ATTR_READ_ONLY  0x01
#define FAT32_ATTR_HIDDEN     0x02
#define FAT32_ATTR_SYSTEM     0x04
#define FAT32_ATTR_VOLUME_ID  0x08
#define FAT32_ATTR_DIRECTORY  0x10
#define FAT32_ATTR_ARCHIVE    0x20
#define FAT32_ATTR_LFN        0x0F

#define FAT32_CLUSTER_FREE    0x00000000
#define FAT32_CLUSTER_EOC     0x0FFFFFF8

#define FAT32_MAX_LFN  256

typedef void (*fat32_list_cb)(const char *name,
                              uint32_t size,
                              uint8_t  attr,
                              uint32_t first_cluster);

typedef struct {
    int      mounted;
    uint32_t part_lba;
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_cluster;
    uint32_t reserved_sectors;
    uint32_t num_fats;
    uint32_t fat_size_sectors;
    uint32_t root_cluster;
    uint32_t total_clusters;
    uint32_t first_fat_sector;
    uint32_t first_data_sector;
    char     volume_label[12];
    char     fs_type[9];
} Fat32Volume;

int  fat32_mount(uint32_t part_lba);
void fat32_unmount(void);
int  fat32_is_mounted(void);
const Fat32Volume *fat32_get_volume(void);

int  fat32_list(const char *path, fat32_list_cb cb);
int  fat32_read_file(const char *path, void *buf, uint32_t max_size);
int  fat32_exists(const char *path);
int  fat32_is_dir(const char *path);
int  fat32_file_size(const char *path);

uint32_t fat32_scan(void);
int fat32_mount(uint32_t part_lba);

#ifdef __cplusplus
}
#endif
