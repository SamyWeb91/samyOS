// ramdisk.cpp
#include "ramdisk.h"
#include "libc.h"
#include "heap.h"

/* =====================================================================
 * Estado
 * ===================================================================== */
typedef struct {
    unsigned char *base;
    unsigned int   size;
    int            used;
    int            dynamic;   /* 1 = kmalloc, 0 = estatico */
} Ramdisk;

static Ramdisk ramdisks[RAMDISK_MAX];
static int ramdisk_ready = 0;

/* =====================================================================
 * Init interno
 * ===================================================================== */
static void ramdisk_init_once(void) {
    if (ramdisk_ready) return;
    for (int i = 0; i < RAMDISK_MAX; i++) {
        ramdisks[i].base = 0;
        ramdisks[i].size = 0;
        ramdisks[i].used = 0;
        ramdisks[i].dynamic = 0;
    }
    ramdisk_ready = 1;
}

/* =====================================================================
 * API publica
 * ===================================================================== */
extern "C" int ramdisk_create_static(unsigned int base, unsigned int size) {
    ramdisk_init_once();
    if (base == 0 || size == 0) return -1;

    for (int i = 0; i < RAMDISK_MAX; i++) {
        if (!ramdisks[i].used) {
            ramdisks[i].base = (unsigned char *)base;
            ramdisks[i].size = size;
            ramdisks[i].used = 1;
            ramdisks[i].dynamic = 0;
            return i;
        }
    }
    return -1;
}

extern "C" int ramdisk_create(unsigned int size) {
    ramdisk_init_once();
    if (size == 0) return -1;

    for (int i = 0; i < RAMDISK_MAX; i++) {
        if (!ramdisks[i].used) {
            unsigned char *buf = (unsigned char *)kmalloc(size);
            if (!buf) return -1;
            ramdisks[i].base = buf;
            ramdisks[i].size = size;
            ramdisks[i].used = 1;
            ramdisks[i].dynamic = 1;
            return i;
        }
    }
    return -1;
}

extern "C" int ramdisk_destroy(int id) {
    ramdisk_init_once();
    if (id < 0 || id >= RAMDISK_MAX) return -1;
    if (!ramdisks[id].used) return -1;

    if (ramdisks[id].dynamic && ramdisks[id].base) {
        kfree(ramdisks[id].base);
    }
    ramdisks[id].base = 0;
    ramdisks[id].size = 0;
    ramdisks[id].used = 0;
    ramdisks[id].dynamic = 0;
    return 0;
}

extern "C" int ramdisk_read(int id, unsigned int offset, void *dst, unsigned int len) {
    ramdisk_init_once();
    if (id < 0 || id >= RAMDISK_MAX) return -1;
    if (!ramdisks[id].used || !dst) return -1;
    if (offset >= ramdisks[id].size) return 0;

    if (offset + len > ramdisks[id].size) {
        len = ramdisks[id].size - offset;
    }
    memcpy(dst, ramdisks[id].base + offset, len);
    return (int)len;
}

extern "C" int ramdisk_write(int id, unsigned int offset, const void *src, unsigned int len) {
    ramdisk_init_once();
    if (id < 0 || id >= RAMDISK_MAX) return -1;
    if (!ramdisks[id].used || !src) return -1;
    if (offset >= ramdisks[id].size) return 0;

    if (offset + len > ramdisks[id].size) {
        len = ramdisks[id].size - offset;
    }
    memcpy(ramdisks[id].base + offset, src, len);
    return (int)len;
}

extern "C" unsigned int ramdisk_size(int id) {
    ramdisk_init_once();
    if (id < 0 || id >= RAMDISK_MAX) return 0;
    if (!ramdisks[id].used) return 0;
    return ramdisks[id].size;
}

extern "C" int ramdisk_exists(int id) {
    ramdisk_init_once();
    if (id < 0 || id >= RAMDISK_MAX) return 0;
    return ramdisks[id].used;
}
