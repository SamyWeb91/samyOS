#pragma once

#include <stdint.h>

// Colores RGB (formato 0x00RRGGBB)
#define GFX_BLACK      0x00000000
#define GFX_WHITE      0x00FFFFFF
#define GFX_RED        0x00FF0000
#define GFX_GREEN      0x0000FF00
#define GFX_BLUE       0x000000FF
#define GFX_YELLOW     0x00FFFF00
#define GFX_CYAN       0x0000FFFF
#define GFX_MAGENTA    0x00FF00FF
#define GFX_GRAY       0x00808080
#define GFX_DARKGRAY   0x00404040

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa el framebuffer (mapea, guarda dimensiones).
void gfx_init(void);

// Retorna 1 si gfx está listo.
int  gfx_ready(void);

// Limpia toda la pantalla con un color.
void gfx_clear(uint32_t color);

// Dibuja un píxel.
void gfx_putpixel(int x, int y, uint32_t color);

// Rectángulo relleno.
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);

// Dibuja un carácter (fuente 8x16).
void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);

// Dibuja un string.
void gfx_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg);

// Info del framebuffer.
int gfx_width(void);
int gfx_height(void);

#ifdef __cplusplus
}
#endif
