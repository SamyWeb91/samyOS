// fat32_min.cpp - Etapa 2
// Anade las variables globales de fat32.cpp pero SIN tocar hardware.

#include "fat32.h"
#include <stdint.h>

// --- Variables globales identicas a fat32.cpp ---
// Si esto rompe el desktop, el problema son las globales.
// Si esto no rompe el desktop, el problema esta en el codigo que las usa.

static Fat32Volume g_vol;
static uint8_t     g_sector_buf[512];
static uint8_t    *g_cluster_buf = 0;
static uint32_t    g_cluster_buf_size = 0;

// --- Funciones externas minimas, sin tocar hardware ---

extern "C" int fat32_mount(uint32_t part_lba) {
    (void)part_lba;
    // No tocamos ahci, no tocamos kmalloc.
    // Solo marcamos como no montado.
    g_vol.mounted = 0;
    return 0;
}

extern "C" void fat32_unmount(void) {
    // Solo limpiamos las globales, sin kfree.
    g_cluster_buf = 0;
    g_cluster_buf_size = 0;
    // Nota: no llamamos a my_memset, porque eso es parte de fat32.cpp.
    // Lo dejamos asi para que el test sea limpio.
}

extern "C" int fat32_is_mounted(void) {
    return g_vol.mounted;
}

extern "C" const Fat32Volume *fat32_get_volume(void) {
    if (!g_vol.mounted) return 0;
    return &g_vol;
}

extern "C" int fat32_list(const char *path, fat32_list_cb cb) {
    (void)path; (void)cb;
    return 0;
}

extern "C" int fat32_read_file(const char *path, void *buf, uint32_t max_size) {
    (void)path; (void)buf; (void)max_size;
    return -1;
}

extern "C" int fat32_exists(const char *path) {
    (void)path;
    return 0;
}

extern "C" int fat32_is_dir(const char *path) {
    (void)path;
    return 0;
}

extern "C" int fat32_file_size(const char *path) {
    (void)path;
    return -1;
}
