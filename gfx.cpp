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
static uint8_t *fb          = 0;
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

static inline uint32_t pack_to_fb(uint32_t c) {
    return ((c & 0x000000FF)      ) |
           ((c & 0x0000FF00)      ) |
           ((c & 0x00FF0000)      ) |
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

    // Mapear TODO el framebuffer: pitch * height, redondeado a KB.
    // PTE_PAT activa el write-combining en esta zona (PAT entrada 1 = WC,
    // configurada por pat_init() en kernel.c antes de gfx_init()).
    uint32_t total_bytes = (uint32_t)fb_pitch * (uint32_t)fb_h;
    uint32_t size_kb = (total_bytes + 0xFFF) / 0x400 + 1;
    paging_map_range(addr, addr, size_kb,
                     PTE_PRESENT | PTE_RW | PTE_PAT);

    fb = (uint8_t *)addr;

    kprintf("gfx: %dx%d %ubpp pitch=%u bytes fb=%x (mapeado %u KB, WC)\n",
            fb_w, fb_h, fb_bpp, fb_pitch, addr, size_kb);
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

// Limpiar TODO el framebuffer, incluyendo el padding de cada linea
extern "C" void gfx_clear(uint32_t color) {
    if (bb) {
        // El backbuffer no tiene padding
        for (int y = 0; y < bb_h; y++) {
            uint32_t *p = bb + (uint32_t)y * bb_w;
            for (int x = 0; x < bb_w; x++) p[x] = color;
        }
        return;
    }

    if (!fb) return;

    // Sin backbuffer: limpiar TODO el framebuffer (fb_pitch * fb_h)
    for (int y = 0; y < fb_h; y++) {
        uint8_t *p = fb + (uint32_t)y * fb_pitch;

        if (fb_bpp == 32) {
            uint32_t *p32 = (uint32_t *)p;
            uint32_t n = (uint32_t)fb_pitch / 4;
            uint32_t c = pack_to_fb(color);
            for (uint32_t x = 0; x < n; x++) p32[x] = c;
        } else if (fb_bpp == 24) {
            uint32_t n = (uint32_t)fb_pitch / 3;
            for (uint32_t x = 0; x < n; x++) {
                p[x*3 + 0] = (color      ) & 0xFF;
                p[x*3 + 1] = (color >>  8) & 0xFF;
                p[x*3 + 2] = (color >> 16) & 0xFF;
            }
        }
    }
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

    /* Límite subido a 16 MB para 1280x1024x32 (5 MB) y similares */
    if (bytes > 16u * 1024u * 1024u) {
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

// Flip COMPLETO: copia el backbuffer al framebuffer real.
// Tambien limpia el padding de cada linea (si fb_pitch > fb_w*bpp/8).
extern "C" void gfx_backbuffer_flip(void) {
    if (!fb || !bb) return;

    for (int y = 0; y < fb_h; y++) {
        uint32_t *src = bb + (uint32_t)y * bb_w;
        uint8_t  *dst = fb + (uint32_t)y * fb_pitch;

        if (fb_bpp == 32) {
            uint32_t *d32 = (uint32_t *)dst;
            for (int x = 0; x < fb_w; x++) {
                d32[x] = src[x];
            }
            // Rellenar el padding de la linea con negro
            uint32_t n = (uint32_t)fb_pitch / 4;
            for (uint32_t x = (uint32_t)fb_w; x < n; x++) d32[x] = 0;
        } else if (fb_bpp == 24) {
            for (int x = 0; x < fb_w; x++) {
                uint32_t c = src[x];
                dst[x*3 + 0] = (c      ) & 0xFF;
                dst[x*3 + 1] = (c >>  8) & 0xFF;
                dst[x*3 + 2] = (c >> 16) & 0xFF;
            }
            uint32_t n = (uint32_t)fb_pitch / 3;
            for (uint32_t x = (uint32_t)fb_w; x < n; x++) {
                dst[x*3 + 0] = 0;
                dst[x*3 + 1] = 0;
                dst[x*3 + 2] = 0;
            }
        }
    }
}

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
        for (int y = ry; y < ry + rh; y++) {
            uint32_t *src = bb + (uint32_t)y * bb_w + rx;
            uint32_t *dst = (uint32_t *)(fb + (uint32_t)y * fb_pitch) + rx;
            for (int x = 0; x < rw; x++) {
                dst[x] = src[x];
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
