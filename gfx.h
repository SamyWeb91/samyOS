#pragma once

#include <stdint.h>

// =====================================================================
// Colores RGB (formato 0x00RRGGBB)
// =====================================================================
#define GFX_BLACK        0x00000000
#define GFX_WHITE        0x00FFFFFF
#define GFX_RED          0x00FF0000
#define GFX_GREEN        0x0000FF00
#define GFX_BLUE         0x000000FF
#define GFX_YELLOW       0x00FFFF00
#define GFX_CYAN         0x0000FFFF
#define GFX_MAGENTA      0x00FF00FF
#define GFX_GRAY         0x00808080
#define GFX_DARKGRAY     0x00404040
#define GFX_LIGHTGRAY    0x00C0C0C0
#define GFX_ORANGE       0x00FFA500
#define GFX_PURPLE       0x00800080
#define GFX_BROWN        0x00A52A2A
#define GFX_PINK         0x00FFC0CB
#define GFX_LIME         0x00BFFF00

#define GFX_RGB(r,g,b)   (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

#ifdef __cplusplus
extern "C" {
#endif

// =====================================================================
// Init
// =====================================================================
void gfx_init(void);
int  gfx_ready(void);

// =====================================================================
// Info
// =====================================================================
int gfx_width(void);
int gfx_height(void);
int gfx_bpp(void);

// =====================================================================
// Primitivas
// =====================================================================
void gfx_putpixel(int x, int y, uint32_t color);
uint32_t gfx_getpixel(int x, int y);
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);
void gfx_clear(uint32_t color);
void gfx_draw_rect(int x, int y, int w, int h, uint32_t color);

void gfx_copy_rect(int src_x, int src_y, int dst_x, int dst_y, int w, int h);

// =====================================================================
// Texto
// =====================================================================
void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void gfx_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg);

// =====================================================================
// Doble buffer
// =====================================================================
int  gfx_backbuffer_init(void);
int  gfx_backbuffer_active(void);
void gfx_backbuffer_flip(void);
void gfx_backbuffer_flip_region(int rx, int ry, int rw, int rh);
void gfx_backbuffer_free(void);

#ifdef __cplusplus
}
#endif
