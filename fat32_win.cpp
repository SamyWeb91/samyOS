// fat32_win.cpp - Etapa 4
// Anade fat32_list para listar el directorio raiz.

#include "fat32.h"
#include "ahci.h"
#include "libc.h"
#include "heap.h"
#include "screen.h"
#include <stdint.h>

#pragma pack(push, 1)

typedef struct {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_num;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
    uint8_t  boot_code[420];
    uint16_t signature;
} Fat32Bpb;

typedef struct {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  reserved_nt;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t first_cluster_hi;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_lo;
    uint32_t file_size;
} Fat32DirEntry;

typedef struct {
    uint8_t  seq;
    uint16_t name1[5];
    uint8_t  attr;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t first_cluster_lo;
    uint16_t name3[2];
} Fat32LfnEntry;

#pragma pack(pop)

static Fat32Volume g_vol = {0};
static uint8_t     g_sector_buf[512] = {0};
static uint8_t    *g_cluster_buf = 0;
static uint32_t    g_cluster_buf_size = 0;

static void local_memset(void *dst, int v, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    for (uint32_t i = 0; i < n; i++) d[i] = (uint8_t)v;
}

static void local_memcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}

static void local_strcpy_n(char *dst, const char *src, int n) {
    int i = 0;
    while (src[i] && i < n - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

// --- Lectura de sectores ---

static int read_sector_part(uint32_t off, void *buf) {
    if (!g_vol.mounted) return 0;
    return ahci_read_sector((uint64_t)(g_vol.part_lba + off), buf);
}

static int read_cluster(uint32_t cluster, void *dst) {
    if (!g_vol.mounted) return 0;
    if (cluster < 2) return 0;
    if (cluster >= g_vol.total_clusters + 2) return 0;

    uint32_t first_sector = g_vol.first_data_sector +
                            (cluster - 2) * g_vol.sectors_per_cluster;

    uint8_t *out = (uint8_t *)dst;
    for (uint32_t s = 0; s < g_vol.sectors_per_cluster; s++) {
        if (!read_sector_part(first_sector + s, g_sector_buf)) return 0;
        local_memcpy(out + s * 512, g_sector_buf, 512);
    }
    return 1;
}

static uint32_t fat_next(uint32_t cluster) {
    if (!g_vol.mounted) return 0;
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = g_vol.first_fat_sector + (fat_offset / 512);
    uint32_t entry_off  = fat_offset % 512;

    if (!read_sector_part(fat_sector, g_sector_buf)) return 0;

    uint32_t v = *(uint32_t *)(g_sector_buf + entry_off);
    return v & 0x0FFFFFFF;
}

static void dir_entry_name(const Fat32DirEntry *e, char *out) {
    int k = 0;
    for (int i = 0; i < 8; i++) {
        if (e->name[i] == ' ') break;
        out[k++] = e->name[i];
    }
    if (e->ext[0] != ' ') {
        out[k++] = '.';
        for (int i = 0; i < 3; i++) {
            if (e->ext[i] == ' ') break;
            out[k++] = e->ext[i];
        }
    }
    out[k] = 0;
}

static uint32_t dir_entry_cluster(const Fat32DirEntry *e) {
    return ((uint32_t)e->first_cluster_hi << 16) | e->first_cluster_lo;
}

// --- fat32_list (solo directorio raiz) ---

extern "C" int fat32_list(const char *path, fat32_list_cb cb)
{
    if (!g_vol.mounted || !cb || !g_cluster_buf) return 0;

    kprintf("[FAT32] list: inicio\n");

    // Solo soportamos "/" por ahora. Cualquier otra ruta devuelve 0.
    if (path && *path && !(path[0] == '/' && path[1] == 0)) {
        kprintf("[FAT32] list: solo se soporta /\n");
        return 0;
    }

    uint32_t cluster = g_vol.root_cluster;
    int safety = 0;
    char lfn_buffer[FAT32_MAX_LFN];
    int  lfn_len = 0;
    int  lfn_active = 0;

    while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC && safety < 100) {
        kprintf("[FAT32] list: leyendo cluster %u\n", cluster);

        if (!read_cluster(cluster, g_cluster_buf)) {
            kprintf("[FAT32] list: fallo leyendo cluster\n");
            return 0;
        }

        uint32_t entries = g_vol.bytes_per_cluster / 32;
        Fat32DirEntry *dir = (Fat32DirEntry *)g_cluster_buf;

        for (uint32_t i = 0; i < entries; i++) {
            uint8_t first = dir[i].name[0];
            if (first == 0x00) return 1;
            if (first == 0xE5) { lfn_active = 0; lfn_len = 0; continue; }

            uint8_t attr = dir[i].attr;

            if ((attr & FAT32_ATTR_LFN) == FAT32_ATTR_LFN) {
                Fat32LfnEntry *lfn = (Fat32LfnEntry *)&dir[i];
                uint8_t seq = lfn->seq & 0x3F;
                uint16_t chars[13];
                for (int j = 0; j < 5; j++) chars[j]      = lfn->name1[j];
                for (int j = 0; j < 6; j++) chars[5 + j]  = lfn->name2[j];
                for (int j = 0; j < 2; j++) chars[11 + j] = lfn->name3[j];
                if (lfn->seq & 0x40) { lfn_len = 0; lfn_active = 1; }
                int base = (seq - 1) * 13;
                for (int j = 0; j < 13; j++) {
                    uint16_t c = chars[j];
                    if (c == 0x0000 || c == 0xFFFF) continue;
                    int pos = base + j;
                    if (pos >= 0 && pos < FAT32_MAX_LFN - 1) {
                        lfn_buffer[pos] = (c < 128) ? (char)c : '?';
                        if (pos + 1 > lfn_len) lfn_len = pos + 1;
                    }
                }
                continue;
            }

            if (attr & FAT32_ATTR_VOLUME_ID) { lfn_active = 0; lfn_len = 0; continue; }

            char name[FAT32_MAX_LFN];
            if (lfn_active && lfn_len > 0) {
                lfn_buffer[lfn_len] = 0;
                local_strcpy_n(name, lfn_buffer, FAT32_MAX_LFN);
            } else {
                dir_entry_name(&dir[i], name);
            }
            lfn_active = 0; lfn_len = 0;

            cb(name, dir[i].file_size, attr, dir_entry_cluster(&dir[i]));
        }
        cluster = fat_next(cluster);
        safety++;
    }
    kprintf("[FAT32] list: fin\n");
    return 1;
}

// --- fat32_mount (igual que Etapa 3) ---

extern "C" int fat32_mount(uint32_t part_lba)
{
    fat32_unmount();

    if (!ahci_is_ready()) return 0;
    if (!ahci_read_sector((uint64_t)part_lba, g_sector_buf)) return 0;

    Fat32Bpb *bpb = (Fat32Bpb *)g_sector_buf;
    if (bpb->signature != 0xAA55) return 0;
    if (bpb->bytes_per_sector != 512) return 0;

    int is_fat32 = 0;
    for (int i = 0; i < 8; i++) {
        if (bpb->fs_type[i] == 'F' && bpb->fs_type[i+1] == 'A' &&
            bpb->fs_type[i+2] == 'T' && bpb->fs_type[i+3] == '3' &&
            bpb->fs_type[i+4] == '2') { is_fat32 = 1; break; }
    }
    if (bpb->fat_size_16 == 0 && bpb->fat_size_32 > 0) is_fat32 = 1;
    if (!is_fat32) return 0;

    g_vol.mounted             = 1;
    g_vol.part_lba            = part_lba;
    g_vol.bytes_per_sector    = bpb->bytes_per_sector;
    g_vol.sectors_per_cluster = bpb->sectors_per_cluster;
    g_vol.bytes_per_cluster   = (uint32_t)bpb->bytes_per_sector * bpb->sectors_per_cluster;
    g_vol.reserved_sectors    = bpb->reserved_sectors;
    g_vol.num_fats            = bpb->num_fats;
    g_vol.fat_size_sectors    = bpb->fat_size_32;
    g_vol.root_cluster        = bpb->root_cluster;
    g_vol.first_fat_sector    = bpb->reserved_sectors;
    g_vol.first_data_sector   = bpb->reserved_sectors +
                                bpb->num_fats * bpb->fat_size_32;

    uint32_t total_sectors = bpb->total_sectors_32;
    if (total_sectors == 0) total_sectors = bpb->total_sectors_16;
    if (total_sectors > g_vol.first_data_sector) {
        g_vol.total_clusters = (total_sectors - g_vol.first_data_sector) /
                               bpb->sectors_per_cluster;
    } else {
        g_vol.total_clusters = 0;
    }

    for (int i = 0; i < 11; i++) g_vol.volume_label[i] = bpb->volume_label[i];
    g_vol.volume_label[11] = 0;
    for (int i = 0; i < 8; i++) g_vol.fs_type[i] = bpb->fs_type[i];
    g_vol.fs_type[8] = 0;

    g_cluster_buf_size = g_vol.bytes_per_cluster;
    if (g_cluster_buf_size == 0 || g_cluster_buf_size > 65536) {
        g_vol.mounted = 0;
        g_cluster_buf_size = 0;
        return 0;
    }

    g_cluster_buf = (uint8_t *)kmalloc(g_cluster_buf_size);
    if (!g_cluster_buf) {
        g_vol.mounted = 0;
        g_cluster_buf_size = 0;
        return 0;
    }
    return 1;
}

extern "C" void fat32_unmount(void) {
    if (g_cluster_buf) {
        kfree(g_cluster_buf);
        g_cluster_buf = 0;
    }
    g_cluster_buf_size = 0;
    local_memset(&g_vol, 0, sizeof(g_vol));
}

extern "C" int fat32_is_mounted(void) { return g_vol.mounted; }
extern "C" const Fat32Volume *fat32_get_volume(void) { return &g_vol; }
extern "C" int fat32_read_file(const char *path, void *buf, uint32_t max_size) { (void)path; (void)buf; (void)max_size; return -1; }
extern "C" int fat32_exists(const char *path) { (void)path; return 0; }
extern "C" int fat32_is_dir(const char *path) { (void)path; return 0; }
extern "C" int fat32_file_size(const char *path) { (void)path; return -1; }
