// wallpaper.cpp
#include "wallpaper.h"
#include "gfx.h"
#include "fat32.h"
#include "heap.h"
#include "screen.h"   // kprintf
#include <stddef.h>
// Lectura segura de enteros little-endian (evita UB por aliasing/padding)
static inline uint32_t rd_u32(const uint8_t *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] <<  8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}
static inline uint16_t rd_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

extern "C" int wallpaper_load(const char *path) {
    if (!path) return 0;

    int fsize = fat32_file_size(path);
    if (fsize <= 0) return 0;

    uint8_t *buf = (uint8_t *)kmalloc((unsigned int)fsize);
    if (!buf) return 0;

    int nread = fat32_read_file(path, buf, fsize);
    if (nread != fsize) {
        kfree(buf);
        return 0;
    }

    if (fsize < 54) { kfree(buf); return 0; }        // cabecera BMP minima

    uint32_t off    = rd_u32(buf + 10);
    uint32_t w      = rd_u32(buf + 18);
    uint32_t h      = rd_u32(buf + 22);
    uint16_t bpp    = rd_u16(buf + 28);

    if (bpp != 24 || w == 0 || h == 0) {
        kfree(buf);
        return 0;
    }

    // BMP es bottom-up. Bliteamos una fila a la vez con gfx_blit_rgb888,
    // que no depende de FAT32 ni toca el backbuffer directamente.
    int stride = (int)((w * 3u + 3u) & ~3u);
    uint32_t draw_h = h;
    if (draw_h > (uint32_t)gfx_height()) draw_h = (uint32_t)gfx_height();
    uint32_t draw_w = w;
    if (draw_w > (uint32_t)gfx_width())  draw_w = (uint32_t)gfx_width();

    for (uint32_t y = 0; y < draw_h; y++) {
        const uint8_t *row = buf + off + (size_t)(h - 1 - y) * (size_t)stride;
        gfx_blit_rgb888(0, (int)y, (int)draw_w, 1, row, stride);
    }

    kfree(buf);
    return 1;
}
