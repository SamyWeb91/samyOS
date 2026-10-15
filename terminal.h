#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TERM_WIN_W  700
#define TERM_WIN_H  450

#define TERM_COLS   80
#define TERM_ROWS   24

/* =====================================================================
 * Variables y funciones del subsistema de ventanas
 * que screen.cpp usa para redirigir la salida al terminal grafico.
 * ===================================================================== */

/* Identificador de la ventana del terminal (definido en terminal.cpp).
   -1 si no hay terminal activo. */
extern int term_wid;

/* Prompt que se dibuja al limpiar la terminal. */
void term_prompt(void);

/* API de ventanas (definidas en win.cpp / composer.cpp). */
void win_sys_clear(int wid);
void win_sys_write(int wid, const char *s, int n);
int  win_get_rect(int wid, int *x, int *y, int *w, int *h);

/* Marca una zona como "sucia" para que el compositor la redibuje. */
void dirty_union(int x, int y, int w, int h);

/* =====================================================================
 * API del terminal
 * ===================================================================== */

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

/* Boton cerrar */
int  terminal_point_in_close(int mx, int my);
void terminal_close(void);
int  terminal_is_visible(void);

/* Arrastrar */
int  terminal_point_in_titlebar(int mx, int my);

/* Redireccion de teclado (ya NO se usa, se deja por compatibilidad) */
int  terminal_handle_key(char c);

#ifdef __cplusplus
}
#endif
