#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TERM_WIN_W  700
#define TERM_WIN_H  450

#define TERM_COLS   80
#define TERM_ROWS   24

void terminal_init(int x, int y, int w, int h);
void terminal_set_pos(int x, int y);
void terminal_get_rect(int *x, int *y, int *w, int *h);

void terminal_clear(void);
void terminal_putchar(char c);
void terminal_puts(const char *s);

void terminal_draw(void);

void terminal_set_focus(int focus);
int  terminal_get_focus(void);

void terminal_blink_tick(void);

// Boton cerrar
int  terminal_point_in_close(int mx, int my);
void terminal_close(void);
int  terminal_is_visible(void);

// Arrastrar
int  terminal_point_in_titlebar(int mx, int my);

// Redireccion de teclado (ya NO se usa, se deja por compatibilidad)
int  terminal_handle_key(char c);

#ifdef __cplusplus
}
#endif
