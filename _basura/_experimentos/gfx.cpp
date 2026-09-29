#include "gfx.h"
#include "screen.h"

static volatile unsigned int *fb = 0;
static unsigned int pitch = 0;

void gfx_init() {
    fb = (volatile unsigned int*)FB_ADDR;
    pitch = FB_PITCH / 4;  // pitch en DWORDs (4 bytes por pixel)
}

void gfx_putpixel(int x, int y, unsigned int color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    fb[y * pitch + x] = color;
}

void gfx_clear(unsigned int color) {
    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            fb[y * pitch + x] = color;
        }
    }
}

void gfx_draw_rect(int x, int y, int w, int h, unsigned int color) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            gfx_putpixel(x + i, y + j, color);
        }
    }
}
