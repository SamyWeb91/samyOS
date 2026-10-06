#include "gfx.h"
#include "paging.h"
#include "screen.h"

static volatile unsigned int *fb = 0;

void gfx_init() {
    unsigned int phys = FB_PHYS_ADDR;
    kprintf("Framebuffer fisico: %x\n", phys);
    kprintf("Mapeando %u KB\n", 3072);

    paging_map_range(phys, phys, 3072, PTE_PRESENT | PTE_RW);

    fb = (volatile unsigned int*)phys;
}

void gfx_putpixel(int x, int y, unsigned int color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    fb[y * SCREEN_W + x] = color;
}

void gfx_clear(unsigned int color) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        fb[i] = color;
    }
}

void gfx_draw_rect(int x, int y, int w, int h, unsigned int color) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            gfx_putpixel(x + i, y + j, color);
        }
    }
}
