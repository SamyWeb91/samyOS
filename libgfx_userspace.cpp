// libgfx_userspace.cpp
// Se compila UNA VEZ como libreria y se enlaza en el ELF de desktop.
// NO se compila en el kernel.
#include "gfx.h"
#include "syscall.h"
#include "sys_user.h"    // tu wrapper syscall() de userspace

static uint32_t *bb_user = 0;
static int bb_w = 0, bb_h = 0;

extern "C" uint8_t font8x16[256][16];

// Wrapper local
static inline int do_sys(int n, int a, int b, int c, int d, int e) {
    return syscall(n, a, b, c, d, e);
}

// =====================================================================
// Init en userspace: mapea el backbuffer del kernel
// =====================================================================
extern "C" void gfx_init(void) {
    struct fb_info info;
    if (do_sys(SYS_FB_INFO, (int)&info, 0, 0, 0, 0) != 0) return;

    unsigned int va = 0;
    if (do_sys(SYS_FB_MAP, (int)&va, 0, 0, 0, 0) != 0) return;

    bb_user = (uint32_t *)va;
    bb_w = info.w;
    bb_h = info.h;
}

extern "C" int  gfx_ready(void)  { return bb_user != 0; }
extern "C" int  gfx_width(void)  { return bb_w; }
extern "C" int  gfx_height(void) { return bb_h; }
extern "C" int  gfx_bpp(void)    { return 32; }

// =====================================================================
// Primitivas (solo escriben al bb_user; nadie toca el FB real aqui)
// =====================================================================
extern "C" void gfx_putpixel(int x, int y, uint32_t c) {
    if (!bb_user || x < 0 || x >= bb_w || y < 0 || y >= bb_h) return;
    bb_user[(uint32_t)y * bb_w + x] = c;
}

extern "C" uint32_t gfx_getpixel(int x, int y) {
    if (!bb_user || x < 0 || x >= bb_w || y < 0 || y >= bb_h) return 0;
    return bb_user[(uint32_t)y * bb_w + x];
}

extern "C" void gfx_fill_rect(int x, int y, int w, int h, uint32_t c) {
    if (!bb_user) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > bb_w) w = bb_w - x;
    if (y + h > bb_h) h = bb_h - y;
    if (w <= 0 || h <= 0) return;
    for (int r = 0; r < h; r++) {
        uint32_t *p = bb_user + (uint32_t)(y + r) * bb_w + x;
        for (int i = 0; i < w; i++) p[i] = c;
    }
}

extern "C" void gfx_clear(uint32_t c) {
    if (!bb_user) return;
    for (int y = 0; y < bb_h; y++) {
        uint32_t *p = bb_user + (uint32_t)y * bb_w;
        for (int x = 0; x < bb_w; x++) p[x] = c;
    }
}

extern "C" void gfx_draw_rect(int x, int y, int w, int h, uint32_t c) {
    gfx_fill_rect(x, y,     w, 1, c);
    gfx_fill_rect(x, y+h-1, w, 1, c);
    gfx_fill_rect(x,     y, 1, h, c);
    gfx_fill_rect(x+w-1, y, 1, h, c);
}

extern "C" void gfx_copy_rect(int sx, int sy, int dx, int dy, int w, int h) {
    if (!bb_user) return;
    if (sx < 0 || sy < 0 || dx < 0 || dy < 0) return;
    if (sx + w > bb_w || dx + w > bb_w) return;
    if (sy + h > bb_h || dy + h > bb_h) return;
    if (dy < sy) {
        for (int r = 0; r < h; r++) {
            uint32_t *s = bb_user + (uint32_t)(sy + r) * bb_w + sx;
            uint32_t *d = bb_user + (uint32_t)(dy + r) * bb_w + dx;
            for (int i = 0; i < w; i++) d[i] = s[i];
        }
    } else {
        for (int r = h - 1; r >= 0; r--) {
            uint32_t *s = bb_user + (uint32_t)(sy + r) * bb_w + sx;
            uint32_t *d = bb_user + (uint32_t)(dy + r) * bb_w + dx;
            for (int i = 0; i < w; i++) d[i] = s[i];
        }
    }
}

extern "C" void gfx_blit_rgb888(int dx, int dy, int w, int h,
                                const uint8_t *src, int stride) {
    if (!bb_user || !src) return;
    if (dx < 0) { src -= dx * 3; w += dx; dx = 0; }
    if (dy < 0) { src -= dy * stride; h += dy; dy = 0; }
    if (dx + w > bb_w) w = bb_w - dx;
    if (dy + h > bb_h) h = bb_h - dy;
    if (w <= 0 || h <= 0) return;
    for (int r = 0; r < h; r++) {
        const uint8_t *s = src + (size_t)r * stride;
        uint32_t *d = bb_user + (uint32_t)(dy + r) * bb_w + dx;
        for (int i = 0; i < w; i++) {
            uint8_t b = s[i*3+0], g = s[i*3+1], rr = s[i*3+2];
            d[i] = ((uint32_t)rr << 16) | ((uint32_t)g << 8) | b;
        }
    }
}

extern "C" void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if (!bb_user || x < 0 || y < 0) return;
    if (x + 8 > bb_w || y + 16 > bb_h) return;
    const uint8_t *g = font8x16[(unsigned char)c];
    for (int row = 0; row < 16; row++) {
        uint8_t bits = g[row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            bb_user[(uint32_t)(y+row) * bb_w + (x+col)] = color;
        }
    }
}

extern "C" void gfx_draw_string(int x, int y, const char *s,
                                uint32_t fg, uint32_t bg) {
    int cx = x, cy = y;
    while (*s) {
        if (*s == '\n') { cx = x; cy += 16; }
        else {
            gfx_draw_char(cx, cy, *s, fg, bg);
            cx += 8;
            if (cx + 8 > bb_w) { cx = x; cy += 16; }
        }
        s++;
    }
}

// =====================================================================
// Flip en userspace = syscall
// =====================================================================
extern "C" int gfx_backbuffer_active(void) { return bb_user != 0; }

extern "C" void gfx_backbuffer_flip(void) {
    do_sys(SYS_FB_FLIP_RECT, 0, 0, bb_w, bb_h, 0);
}

extern "C" void gfx_backbuffer_flip_region(int x, int y, int w, int h) {
    do_sys(SYS_FB_FLIP_RECT, x, y, w, h, 0);
}

// Stubs para no romper enlaces si algún otro módulo los referencia
extern "C" int  gfx_backbuffer_init(void) { return gfx_ready(); }
extern "C" void gfx_backbuffer_free(void) {}
extern "C" uint32_t *gfx_backbuffer_va(void) { return bb_user; }
extern "C" int  gfx_backbuffer_width(void)  { return bb_w; }
extern "C" int  gfx_backbuffer_height(void) { return bb_h; }
