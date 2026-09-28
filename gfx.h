#pragma once

#define FB_ADDR   (*(volatile unsigned int*)0x5800)
#define FB_PITCH  (*(volatile unsigned short*)0x5804)
#define SCREEN_W  1024
#define SCREEN_H  768

void gfx_init();
void gfx_putpixel(int x, int y, unsigned int color);
void gfx_clear(unsigned int color);
void gfx_draw_rect(int x, int y, int w, int h, unsigned int color);
