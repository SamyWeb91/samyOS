#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TERM_COLS   80
#define TERM_ROWS   24

// =====================================================================
// Estado interno del terminal (buffer + cursor)
// Se pasa como puntero a window_add_terminal para que cada ventana
// terminal pueda tener su propio buffer.
// =====================================================================
struct TerminalState {
    char term_buf[TERM_ROWS][TERM_COLS];
    int  term_cx;
    int  term_cy;

    int  cursor_visible;
    unsigned int cursor_last_tick;

    int  has_focus;
};

// =====================================================================
// Ciclo de vida
// =====================================================================
void terminal_init_state(struct TerminalState *s);
void terminal_clear_state(struct TerminalState *s);

void terminal_putchar_state(struct TerminalState *s, char c);
void terminal_puts_state(struct TerminalState *s, const char *str);

// Dibuja el contenido del estado en un rectangulo dado (coords absolutas).
void terminal_draw_in_rect(struct TerminalState *s, int x, int y, int w, int h);

// Parpadeo
void terminal_blink_state(struct TerminalState *s);
void terminal_set_focus_state(struct TerminalState *s, int focus);

#ifdef __cplusplus
}
#endif
