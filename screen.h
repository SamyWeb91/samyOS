#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void screen_clear();
void screen_putchar(char c);
void screen_write(const char *str);
void kprintf(const char *fmt, ...);

void screen_set_gfx_mode(void);
void screen_set_colors(unsigned int fg, unsigned int bg);
void screen_blink_tick(void);

// Redirigir screen_putchar/screen_write a la terminal
void screen_set_terminal_mode(int on);
int  screen_get_terminal_mode(void);

// Implementadas en desktop.cpp
void desktop_terminal_putchar(char c);
void desktop_terminal_clear(void);

#ifdef __cplusplus
}
#endif
