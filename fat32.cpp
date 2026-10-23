// fat32.cpp
// Driver FAT32 en modo SOLO LECTURA.

#include "fat32.h"
#include "ahci.h"
#include "libc.h"
#include "heap.h"

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

static Fat32Volume g_vol;
static uint8_t     g_sector_buf[512];
static uint8_t    *g_cluster_buf = 0;
static uint32_t    g_cluster_buf_size = 0;

static void my_strcpy_n(char *dst, const char *src, int n) {
    int i = 0;
    while (src[i] && i < n - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static int my_streq_ci(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static void my_memcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}

static void my_memset(void *dst, int v, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    for (uint32_t i = 0; i < n; i++) d[i] = (uint8_t)v;
}

static int read_sector_abs(uint32_t lba, void *buf) {
    return ahci_read_sector((uint64_t)lba, buf);
}

static int read_sector_part(uint32_t off, void *buf) {
    return read_sector_abs(g_vol.part_lba + off, buf);
}

static int read_cluster(uint32_t cluster, void *dst) {
    if (cluster < 2) return 0;
    if (cluster >= g_vol.total_clusters + 2) return 0;

    uint32_t first_sector = g_vol.first_data_sector +
                            (cluster - 2) * g_vol.sectors_per_cluster;

    uint8_t *out = (uint8_t *)dst;
    for (uint32_t s = 0; s < g_vol.sectors_per_cluster; s++) {
        if (!read_sector_part(first_sector + s, g_sector_buf)) return 0;
        my_memcpy(out + s * 512, g_sector_buf, 512);
    }
    return 1;
}

static uint32_t fat_next(uint32_t cluster) {
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

static int find_in_dir(uint32_t dir_cluster,
                       const char *find_name,
                       Fat32DirEntry *out_entry)
{
    if (g_cluster_buf_size == 0) return 0;

    uint32_t cluster = dir_cluster;
    int safety = 0;

    while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC && safety < 10000) {
        if (!read_cluster(cluster, g_cluster_buf)) return 0;

        uint32_t entries_per_cluster = g_vol.bytes_per_cluster / 32;
        Fat32DirEntry *dir = (Fat32DirEntry *)g_cluster_buf;

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (dir[i].name[0] == 0x00) return 0;
            if (dir[i].name[0] == 0xE5) continue;
            if ((dir[i].attr & FAT32_ATTR_LFN) == FAT32_ATTR_LFN) continue;
            if (dir[i].attr & FAT32_ATTR_VOLUME_ID) continue;

            char name[16];
            dir_entry_name(&dir[i], name);
            if (my_streq_ci(name, find_name)) {
                my_memcpy(out_entry, &dir[i], sizeof(Fat32DirEntry));
                return 1;
            }
        }
        cluster = fat_next(cluster);
        safety++;
    }
    return 0;
}

static void to_83(const char *in, char out83[11]) {
    for (int i = 0; i < 11; i++) out83[i] = ' ';
    int k = 0, i = 0;
    while (in[i] && in[i] != '.' && k < 8) {
        char c = in[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        out83[k++] = c;
        i++;
    }
    while (in[i] && in[i] != '.') i++;
    if (in[i] == '.') {
        i++;
        int e = 0;
        while (in[i] && e < 3) {
            char c = in[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            out83[8 + e] = c;
            e++;
            i++;
        }
    }
}

static int resolve_path(const char *path,
                        uint32_t *out_parent_cluster,
                        char *out_last_name,
                        int *out_is_root)
{
    if (!path || !*path) path = "/";
    while (*path == '/') path++;
    if (*path == 0) {
        *out_parent_cluster = g_vol.root_cluster;
        out_last_name[0] = 0;
        *out_is_root = 1;
        return 1;
    }

    uint32_t current = g_vol.root_cluster;
    char component[FAT32_MAX_LFN];

    while (*path) {
        int k = 0;
        while (*path && *path != '/' && k < FAT32_MAX_LFN - 1) {
            component[k++] = *path++;
        }
        component[k] = 0;
        while (*path == '/') path++;

        if (*path == 0) {
            my_strcpy_n(out_last_name, component, FAT32_MAX_LFN);
            *out_parent_cluster = current;
            *out_is_root = 0;
            return 1;
        }

        char name83[11];
        to_83(component, name83);
        Fat32DirEntry e;
        if (!find_in_dir(current, name83, &e)) return 0;
        if ((e.attr & FAT32_ATTR_DIRECTORY) == 0) return 0;
        current = dir_entry_cluster(&e);
        if (current == 0) current = g_vol.root_cluster;
    }
    return 0;
}

extern "C" int fat32_mount(uint32_t part_lba)
{
    fat32_unmount();
    if (!ahci_is_ready()) return 0;
    if (!read_sector_abs(part_lba, g_sector_buf)) return 0;

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
    g_cluster_buf = (uint8_t *)kmalloc(g_cluster_buf_size);
    if (!g_cluster_buf) {
        g_vol.mounted = 0;
        g_cluster_buf_size = 0;
        return 0;
    }
    return 1;
}

extern "C" void fat32_unmount(void)
{
    if (g_cluster_buf) {
        kfree(g_cluster_buf);
        g_cluster_buf = 0;
    }
    g_cluster_buf_size = 0;
    my_memset(&g_vol, 0, sizeof(g_vol));
}

extern "C" int fat32_is_mounted(void) { return g_vol.mounted; }

extern "C" const Fat32Volume *fat32_get_volume(void) {
    if (!g_vol.mounted) return 0;
    return &g_vol;
}

extern "C" int fat32_list(const char *path, fat32_list_cb cb)
{
    if (!g_vol.mounted || !cb) return 0;

    uint32_t parent;
    char last[FAT32_MAX_LFN];
    int is_root = 0;
    if (!resolve_path(path, &parent, last, &is_root)) return 0;

    uint32_t dir_cluster;
    if (is_root) {
        dir_cluster = g_vol.root_cluster;
    } else {
        char name83[11];
        to_83(last, name83);
        Fat32DirEntry e;
        if (!find_in_dir(parent, name83, &e)) return 0;
        if ((e.attr & FAT32_ATTR_DIRECTORY) == 0) return 0;
        dir_cluster = dir_entry_cluster(&e);
    }

    uint32_t cluster = dir_cluster;
    int safety = 0;
    char lfn_buffer[FAT32_MAX_LFN];
    int  lfn_len = 0;
    int  lfn_active = 0;

    while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC && safety < 10000) {
        if (!read_cluster(cluster, g_cluster_buf)) return 0;

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
                my_strcpy_n(name, lfn_buffer, FAT32_MAX_LFN);
            } else {
                dir_entry_name(&dir[i], name);
            }
            lfn_active = 0; lfn_len = 0;

            cb(name, dir[i].file_size, attr, dir_entry_cluster(&dir[i]));
        }
        cluster = fat_next(cluster);
        safety++;
    }
    return 1;
}

extern "C" int fat32_read_file(const char *path, void *buf, uint32_t max_size)
{
    if (!g_vol.mounted || !buf) return -1;

    uint32_t parent;
    char last[FAT32_MAX_LFN];
    int is_root = 0;
    if (!resolve_path(path, &parent, last, &is_root)) return -1;
    if (is_root) return -1;

    char name83[11];
    to_83(last, name83);
    Fat32DirEntry e;
    if (!find_in_dir(parent, name83, &e)) return -1;
    if (e.attr & FAT32_ATTR_DIRECTORY) return -1;

    uint32_t file_size = e.file_size;
    if (file_size > max_size) file_size = max_size;

    uint32_t cluster = dir_entry_cluster(&e);
    uint32_t copied = 0;
    int safety = 0;
    uint8_t *out = (uint8_t *)buf;

    while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC &&
           copied < file_size && safety < 100000) {
        if (!read_cluster(cluster, g_cluster_buf)) return -1;
        uint32_t to_copy = g_vol.bytes_per_cluster;
        if (to_copy > file_size - copied) to_copy = file_size - copied;
        my_memcpy(out + copied, g_cluster_buf, to_copy);
        copied += to_copy;
        cluster = fat_next(cluster);
        safety++;
    }
    return (int)copied;
}

extern "C" int fat32_exists(const char *path)
{
    if (!g_vol.mounted) return 0;
    uint32_t parent;
    char last[FAT32_MAX_LFN];
    int is_root = 0;
    if (!resolve_path(path, &parent, last, &is_root)) return 0;
    if (is_root) return 1;
    char name83[11];
    to_83(last, name83);
    Fat32DirEntry e;
    return find_in_dir(parent, name83, &e);
}

extern "C" int fat32_is_dir(const char *path)
{
    if (!g_vol.mounted) return 0;
    uint32_t parent;
    char last[FAT32_MAX_LFN];
    int is_root = 0;
    if (!resolve_path(path, &parent, last, &is_root)) return 0;
    if (is_root) return 1;
    char name83[11];
    to_83(last, name83);
    Fat32DirEntry e;
    if (!find_in_dir(parent, name83, &e)) return 0;
    return (e.attr & FAT32_ATTR_DIRECTORY) ? 1 : 0;
}

extern "C" int fat32_file_size(const char *path)
{
    if (!g_vol.mounted) return -1;
    uint32_t parent;
    char last[FAT32_MAX_LFN];
    int is_root = 0;
    if (!resolve_path(path, &parent, last, &is_root)) return -1;
    if (is_root) return -1;
    char name83[11];
    to_83(last, name83);
    Fat32DirEntry e;
    if (!find_in_dir(parent, name83, &e)) return -1;
    if (e.attr & FAT32_ATTR_DIRECTORY) return -1;
    return (int)e.file_size;
}
