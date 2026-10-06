#pragma once

void screen_clear();
void screen_putchar(char c);
void screen_write(const char *str);
void kprintf(const char *fmt, ...);

// Modo gráfico (llamar tras gfx_init si VBE está disponible)
void screen_set_gfx_mode(void);

// Colores de texto (opcional)
void screen_set_colors(unsigned int fg, unsigned int bg);

// Cursor parpadeante (llamar desde el bucle de la shell)
void screen_blink_tick(void);
