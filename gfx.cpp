#include "gfx.h"
#include "vbe.h"
#include "screen.h"
#include "paging.h"

// Fuente 8x16 exportada por font8x16.c
extern "C" uint8_t font8x16[256][16];

// =====================================================================
// Estado del driver
// =====================================================================
static uint8_t *fb          = 0;   // framebuffer (byte pointer)
static int      fb_w        = 0;   // ancho en píxeles
static int      fb_h        = 0;   // alto en píxeles
static int      fb_pitch    = 0;   // bytes por línea (del VBE)
static int      fb_bpp      = 0;   // bits por píxel (24 o 32)
static int      fb_bytes_pp = 0;   // bytes por píxel (3 o 4)

// =====================================================================
// Helpers internos
// =====================================================================

// Escribe un píxel en la dirección p según el bpp actual.
static inline void putpixel_at(uint8_t *p, uint32_t color) {
    if (fb_bpp == 32) {
        p[0] = (color      ) & 0xFF;   // B
        p[1] = (color >>  8) & 0xFF;   // G
        p[2] = (color >> 16) & 0xFF;   // R
        p[3] = 0;                      // X
    } else {
        p[0] = (color      ) & 0xFF;   // B
        p[1] = (color >>  8) & 0xFF;   // G
        p[2] = (color >> 16) & 0xFF;   // R
    }
}

// Lee un píxel de la dirección p y lo devuelve en formato 0x00RRGGBB.
static inline uint32_t getpixel_at(const uint8_t *p) {
    uint32_t b = p[0];
    uint32_t g = p[1];
    uint32_t r = p[2];
    return (r << 16) | (g << 8) | b;
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
    if (!fb) return;
    if (x < 0 || x >= fb_w || y < 0 || y >= fb_h) return;

    uint8_t *p = fb + (uint32_t)y * fb_pitch + (uint32_t)x * fb_bytes_pp;
    putpixel_at(p, color);
}

extern "C" uint32_t gfx_getpixel(int x, int y) {
    if (!fb) return 0;
    if (x < 0 || x >= fb_w || y < 0 || y >= fb_h) return 0;

    const uint8_t *p = fb + (uint32_t)y * fb_pitch
                          + (uint32_t)x * fb_bytes_pp;
    return getpixel_at(p);
}

// =====================================================================
// Rectángulo relleno
// =====================================================================
extern "C" void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!fb) return;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fb_w) w = fb_w - x;
    if (y + h > fb_h) h = fb_h - y;
    if (w <= 0 || h <= 0) return;

    for (int row = 0; row < h; row++) {
        uint8_t *p = fb + (uint32_t)(y + row) * fb_pitch
                        + (uint32_t)x * fb_bytes_pp;
        for (int col = 0; col < w; col++) {
            putpixel_at(p, color);
            p += fb_bytes_pp;
        }
    }
}

extern "C" void gfx_clear(uint32_t color) {
    if (!fb) return;
    gfx_fill_rect(0, 0, fb_w, fb_h, color);
}

// =====================================================================
// Marco (rectángulo sin relleno)
// =====================================================================
extern "C" void gfx_draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (!fb) return;
    gfx_fill_rect(x, y,         w, 1, color);
    gfx_fill_rect(x, y + h - 1, w, 1, color);
    gfx_fill_rect(x,         y, 1, h, color);
    gfx_fill_rect(x + w - 1, y, 1, h, color);
}

// =====================================================================
// Copiar región del framebuffer (para scroll)
// =====================================================================
extern "C" void gfx_copy_rect(int src_x, int src_y,
                              int dst_x, int dst_y,
                              int w, int h) {
    if (!fb) return;
    if (w <= 0 || h <= 0) return;

    if (src_x < 0 || src_y < 0 || dst_x < 0 || dst_y < 0) return;
    if (src_x + w > fb_w || dst_x + w > fb_w) return;
    if (src_y + h > fb_h || dst_y + h > fb_h) return;

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

// =====================================================================
// Texto
// =====================================================================
extern "C" void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if (!fb) return;

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
