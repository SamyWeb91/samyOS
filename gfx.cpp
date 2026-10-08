#include "gfx.h"
#include "vbe.h"
#include "screen.h"
#include "paging.h"
#include "heap.h"

// Fuente 8x16 exportada por font8x16.c
extern "C" uint8_t font8x16[256][16];

// =====================================================================
// Estado del driver
// =====================================================================
static uint8_t *fb          = 0;   // framebuffer real (VBE)
static int      fb_w        = 0;
static int      fb_h        = 0;
static int      fb_pitch    = 0;
static int      fb_bpp      = 0;
static int      fb_bytes_pp = 0;

// Backbuffer
static uint32_t *bb         = 0;
static int       bb_w       = 0;
static int       bb_h       = 0;

// =====================================================================
// Helpers
// =====================================================================
static inline void putpixel_at(uint8_t *p, uint32_t color) {
    if (fb_bpp == 32) {
        p[0] = (color      ) & 0xFF;
        p[1] = (color >>  8) & 0xFF;
        p[2] = (color >> 16) & 0xFF;
        p[3] = 0;
    } else {
        p[0] = (color      ) & 0xFF;
        p[1] = (color >>  8) & 0xFF;
        p[2] = (color >> 16) & 0xFF;
    }
}

static inline uint32_t getpixel_at(const uint8_t *p) {
    uint32_t b = p[0];
    uint32_t g = p[1];
    uint32_t r = p[2];
    return (r << 16) | (g << 8) | b;
}

// Convertir 0x00RRGGBB a formato nativo del framebuffer (BGRA en LE)
static inline uint32_t pack_to_fb(uint32_t c) {
    // c = 0x00RRGGBB
    // resultado en memoria: [B, G, R, X] → 0x00RRGGBB se escribe como
    // uint32_t: (R<<16) | (G<<8) | B ... pero en LE se lee byte 0 = B.
    // Para escribir uint32_t de golpe y que en memoria quede B,G,R,X:
    // uint32_t en LE = B | (G<<8) | (R<<16) | (0<<24)
    return ((c & 0x000000FF)      ) |   // B → byte 0
           ((c & 0x0000FF00)      ) |   // G → byte 1
           ((c & 0x00FF0000)      ) |   // R → byte 2
           (0u);
}

// =====================================================================
// Init
// =====================================================================
extern "C" void gfx_init(void) {
    if (!vbe_available()) {
        kprintf("gfx: VBE no disponible\n");
        fb = 0;
        return;
    }

    uint32_t addr = vbe_fb_addr();
    fb_w     = vbe_width();
    fb_h     = vbe_height();
    fb_pitch = vbe_pitch();
    fb_bpp   = vbe_bpp();

    if (fb_bpp == 32)      fb_bytes_pp = 4;
    else if (fb_bpp == 24) fb_bytes_pp = 3;
    else {
        kprintf("gfx: bpp=%u no soportado\n", fb_bpp);
        fb = 0;
        return;
    }

    uint32_t size_mb = (fb_pitch * fb_h + 0xFFFFF) / 0x100000 + 1;
    paging_map_range(addr, addr, size_mb * 1024, PTE_PRESENT | PTE_RW);

    fb = (uint8_t *)addr;

    kprintf("gfx: %dx%d %ubpp pitch=%u bytes fb=%x (mapeado %u MB)\n",
            fb_w, fb_h, fb_bpp, fb_pitch, addr, size_mb);
}

extern "C" int gfx_ready(void)      { return fb != 0; }
extern "C" int gfx_width(void)      { return fb_w; }
extern "C" int gfx_height(void)     { return fb_h; }
extern "C" int gfx_bpp(void)        { return fb_bpp; }

// =====================================================================
// Pixel
// =====================================================================
extern "C" void gfx_putpixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= fb_w || y < 0 || y >= fb_h) return;

    if (bb) {
        bb[(uint32_t)y * bb_w + x] = color;
    } else if (fb) {
        uint8_t *p = fb + (uint32_t)y * fb_pitch
                        + (uint32_t)x * fb_bytes_pp;
        putpixel_at(p, color);
    }
}

extern "C" uint32_t gfx_getpixel(int x, int y) {
    if (x < 0 || x >= fb_w || y < 0 || y >= fb_h) return 0;

    if (bb) {
        return bb[(uint32_t)y * bb_w + x];
    } else if (fb) {
        const uint8_t *p = fb + (uint32_t)y * fb_pitch
                              + (uint32_t)x * fb_bytes_pp;
        return getpixel_at(p);
    }
    return 0;
}

// =====================================================================
// Rectángulo relleno
// =====================================================================
extern "C" void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fb_w) w = fb_w - x;
    if (y + h > fb_h) h = fb_h - y;
    if (w <= 0 || h <= 0) return;

    if (bb) {
        for (int row = 0; row < h; row++) {
            uint32_t *p = bb + (uint32_t)(y + row) * bb_w + x;
            for (int col = 0; col < w; col++) {
                p[col] = color;
            }
        }
    } else if (fb) {
        for (int row = 0; row < h; row++) {
            uint8_t *p = fb + (uint32_t)(y + row) * fb_pitch
                            + (uint32_t)x * fb_bytes_pp;
            for (int col = 0; col < w; col++) {
                putpixel_at(p, color);
                p += fb_bytes_pp;
            }
        }
    }
}

extern "C" void gfx_clear(uint32_t color) {
    gfx_fill_rect(0, 0, fb_w, fb_h, color);
}

// =====================================================================
// Marco
// =====================================================================
extern "C" void gfx_draw_rect(int x, int y, int w, int h, uint32_t color) {
    gfx_fill_rect(x, y,         w, 1, color);
    gfx_fill_rect(x, y + h - 1, w, 1, color);
    gfx_fill_rect(x,         y, 1, h, color);
    gfx_fill_rect(x + w - 1, y, 1, h, color);
}

// =====================================================================
// Copiar región
// =====================================================================
extern "C" void gfx_copy_rect(int src_x, int src_y,
                              int dst_x, int dst_y,
                              int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (src_x < 0 || src_y < 0 || dst_x < 0 || dst_y < 0) return;
    if (src_x + w > fb_w || dst_x + w > fb_w) return;
    if (src_y + h > fb_h || dst_y + h > fb_h) return;

    if (bb) {
        if (dst_y < src_y) {
            for (int row = 0; row < h; row++) {
                uint32_t *s = bb + (uint32_t)(src_y + row) * bb_w + src_x;
                uint32_t *d = bb + (uint32_t)(dst_y + row) * bb_w + dst_x;
                for (int i = 0; i < w; i++) d[i] = s[i];
            }
        } else {
            for (int row = h - 1; row >= 0; row--) {
                uint32_t *s = bb + (uint32_t)(src_y + row) * bb_w + src_x;
                uint32_t *d = bb + (uint32_t)(dst_y + row) * bb_w + dst_x;
                for (int i = 0; i < w; i++) d[i] = s[i];
            }
        }
    } else if (fb) {
        int bytes = w * fb_bytes_pp;
        if (dst_y < src_y) {
            for (int row = 0; row < h; row++) {
                uint8_t *s = fb + (uint32_t)(src_y + row) * fb_pitch
                                + (uint32_t)src_x * fb_bytes_pp;
                uint8_t *d = fb + (uint32_t)(dst_y + row) * fb_pitch
                                + (uint32_t)dst_x * fb_bytes_pp;
                for (int i = 0; i < bytes; i++) d[i] = s[i];
            }
        } else {
            for (int row = h - 1; row >= 0; row--) {
                uint8_t *s = fb + (uint32_t)(src_y + row) * fb_pitch
                                + (uint32_t)src_x * fb_bytes_pp;
                uint8_t *d = fb + (uint32_t)(dst_y + row) * fb_pitch
                                + (uint32_t)dst_x * fb_bytes_pp;
                for (int i = 0; i < bytes; i++) d[i] = s[i];
            }
        }
    }
}

// =====================================================================
// Texto
// =====================================================================
extern "C" void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    unsigned char uc = (unsigned char)c;
    const uint8_t *glyph = font8x16[uc];

    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            gfx_putpixel(x + col, y + row, color);
        }
    }
}

extern "C" void gfx_draw_string(int x, int y, const char *s,
                                uint32_t fg, uint32_t bg) {
    int cx = x;
    int cy = y;
    while (*s) {
        if (*s == '\n') {
            cx = x;
            cy += 16;
        } else {
            gfx_draw_char(cx, cy, *s, fg, bg);
            cx += 8;
            if (cx + 8 > fb_w) {
                cx = x;
                cy += 16;
            }
        }
        s++;
    }
}

// =====================================================================
// Doble buffer
// =====================================================================
extern "C" int gfx_backbuffer_init(void) {
    if (!fb) return 0;
    if (bb) return 1;

    unsigned int bytes = (unsigned int)fb_w * fb_h * sizeof(uint32_t);

    if (bytes > 4u * 1024u * 1024u) {
        kprintf("gfx: backbuffer demasiado grande (%u bytes), no lo creo\n",
                bytes);
        return 0;
    }

    bb = (uint32_t *)kmalloc(bytes);
    if (!bb) {
        kprintf("gfx: kmalloc devolvio 0 para backbuffer (%u bytes)\n", bytes);
        return 0;
    }

    unsigned int addr = (unsigned int)bb;
    if (addr < 0x100000u || addr > 0x8000000u) {
        kprintf("gfx: backbuffer en direccion sospechosa %x, no lo uso\n",
                addr);
        bb = 0;
        return 0;
    }

    // Sanity check: escribir primer y último uint32_t
    volatile uint32_t *test = bb;
    test[0] = 0;
    test[bytes / 4 - 1] = 0;

    bb_w = fb_w;
    bb_h = fb_h;

    kprintf("gfx: backbuffer %dx%d (%u bytes, %u KB) en %x\n",
            bb_w, bb_h, bytes, bytes / 1024, addr);
    return 1;
}

extern "C" int gfx_backbuffer_active(void) {
    return bb != 0;
}

extern "C" void gfx_backbuffer_flip(void) {
    if (!fb || !bb) return;

    for (int y = 0; y < fb_h; y++) {
        uint32_t *src = bb + (uint32_t)y * bb_w;
        uint8_t  *dst = fb + (uint32_t)y * fb_pitch;

        if (fb_bpp == 32) {
            uint32_t *d32 = (uint32_t *)dst;
            for (int x = 0; x < fb_w; x++) {
                d32[x] = pack_to_fb(src[x]);
            }
        } else if (fb_bpp == 24) {
            for (int x = 0; x < fb_w; x++) {
                uint32_t c = src[x];
                dst[x*3 + 0] = (c      ) & 0xFF;
                dst[x*3 + 1] = (c >>  8) & 0xFF;
                dst[x*3 + 2] = (c >> 16) & 0xFF;
            }
        }
    }
}

// =====================================================================
// Flip de una región: versión OPTIMIZADA
//
// En fb_bpp == 32 escribe 32 bits de golpe (1 escritura por píxel).
// En fb_bpp == 24, escribe 3 bytes (no hay otra).
//
// Esto elimina el cuelgue aparente al arrastrar/cerrar ventanas.
// =====================================================================
extern "C" void gfx_backbuffer_flip_region(int rx, int ry, int rw, int rh) {
    if (!fb || !bb) return;

    int W = fb_w;
    int H = fb_h;

    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > W) rw = W - rx;
    if (ry + rh > H) rh = H - ry;
    if (rw <= 0 || rh <= 0) return;

    if (fb_bpp == 32) {
        // Camino rápido: 32 bits por píxel
        for (int y = ry; y < ry + rh; y++) {
            uint32_t *src = bb + (uint32_t)y * bb_w + rx;
            uint32_t *dst = (uint32_t *)(fb + (uint32_t)y * fb_pitch) + rx;
            for (int x = 0; x < rw; x++) {
                dst[x] = pack_to_fb(src[x]);
            }
        }
    } else if (fb_bpp == 24) {
        for (int y = ry; y < ry + rh; y++) {
            uint32_t *src = bb + (uint32_t)y * bb_w + rx;
            uint8_t  *dst = fb + (uint32_t)y * fb_pitch
                               + (uint32_t)rx * 3;
            for (int x = 0; x < rw; x++) {
                uint32_t c = src[x];
                dst[x*3 + 0] = (c      ) & 0xFF;
                dst[x*3 + 1] = (c >>  8) & 0xFF;
                dst[x*3 + 2] = (c >> 16) & 0xFF;
            }
        }
    }
}

extern "C" void gfx_backbuffer_free(void) {
    if (bb) {
        kfree(bb);
        bb = 0;
        bb_w = 0;
        bb_h = 0;
    }
}
