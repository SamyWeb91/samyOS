#pragma once

void screen_clear();
void screen_putchar(char c);
void screen_write(const char *str);
void kprintf(const char *fmt, ...);

void screen_set_gfx_mode(void);
void screen_set_colors(unsigned int fg, unsigned int bg);
void screen_blink_tick(void);

// NUEVO: redirigir screen_putchar/screen_write a la terminal
void screen_set_terminal_mode(int on);
int  screen_get_terminal_mode(void);
