#include "gfx.h"
#include "vbe.h"
#include "screen.h"
#include "paging.h"

extern "C" uint8_t font8x16[256][16];

static uint32_t *fb = 0;
static int      fb_w = 0;
static int      fb_h = 0;
static int      fb_pitch_pixels = 0;

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
    fb_w = vbe_width();
    fb_h = vbe_height();
    fb_pitch_pixels = vbe_pitch() / 4;   // bpp = 32 → 4 bytes/pixel

    // Mapear el framebuffer (puede estar en 0xE0000000, 0xFD000000, etc.)
    // Mapear 4 MB para cubrir 1024x768x32
    paging_map_range(addr, addr, 4096, PTE_PRESENT | PTE_RW);

    fb = (uint32_t *)addr;

    kprintf("gfx: %dx%d, pitch=%d, fb=%x\n",
            fb_w, fb_h, vbe_pitch(), addr);
}

extern "C" int gfx_ready(void) {
    return fb != 0;
}

extern "C" int gfx_width(void)  { return fb_w; }
extern "C" int gfx_height(void) { return fb_h; }

// =====================================================================
// Primitivas
// =====================================================================
extern "C" void gfx_putpixel(int x, int y, uint32_t color) {
    if (!fb) return;
    if (x < 0 || x >= fb_w || y < 0 || y >= fb_h) return;
    fb[y * fb_pitch_pixels + x] = color;
}

extern "C" void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!fb) return;

    // Recortar a la pantalla
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fb_w) w = fb_w - x;
    if (y + h > fb_h) h = fb_h - y;
    if (w <= 0 || h <= 0) return;

    for (int row = 0; row < h; row++) {
        uint32_t *line = fb + (y + row) * fb_pitch_pixels + x;
        for (int col = 0; col < w; col++) {
            line[col] = color;
        }
    }
}

extern "C" void gfx_clear(uint32_t color) {
    if (!fb) return;
    gfx_fill_rect(0, 0, fb_w, fb_h, color);
}

// =====================================================================
// Texto (fuente 8x16)
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

extern "C" void gfx_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg) {
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
