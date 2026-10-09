#include "terminal.h"
#include "gfx.h"
#include "pit.h"

// =====================================================================
// Colores
// =====================================================================
#define TERM_BG        0x000C0C0C
#define TERM_FG        0x00EEEEEE
#define TERM_PROMPT    0x0040FF40

#define CHAR_W  8
#define CHAR_H  16

#define CURSOR_BLINK_TICKS 50

// =====================================================================
// Scroll
// =====================================================================
static void term_scroll_state(struct TerminalState *s) {
    for (int row = 0; row < TERM_ROWS - 1; row++) {
        for (int col = 0; col < TERM_COLS; col++) {
            s->term_buf[row][col] = s->term_buf[row + 1][col];
        }
    }
    for (int col = 0; col < TERM_COLS; col++) {
        s->term_buf[TERM_ROWS - 1][col] = ' ';
    }
    if (s->term_cy > 0) s->term_cy--;
}

// =====================================================================
// Init / Clear
// =====================================================================
extern "C" void terminal_init_state(struct TerminalState *s) {
    terminal_clear_state(s);
    s->has_focus = 1;
    s->cursor_visible = 1;
    s->cursor_last_tick = pit_get_ticks();
}

extern "C" void terminal_clear_state(struct TerminalState *s) {
    for (int row = 0; row < TERM_ROWS; row++) {
        for (int col = 0; col < TERM_COLS; col++) {
            s->term_buf[row][col] = ' ';
        }
    }
    s->term_cx = 0;
    s->term_cy = 0;
    s->cursor_visible = 1;
    s->cursor_last_tick = pit_get_ticks();
}

// =====================================================================
// Putchar
// =====================================================================
extern "C" void terminal_putchar_state(struct TerminalState *s, char c) {
    if (c == '\n') {
        s->term_cx = 0;
        s->term_cy++;
        if (s->term_cy >= TERM_ROWS) term_scroll_state(s);
        return;
    }
    if (c == '\r') {
        s->term_cx = 0;
        return;
    }
    if (c == '\b') {
        if (s->term_cx > 0) {
            s->term_cx--;
            s->term_buf[s->term_cy][s->term_cx] = ' ';
        }
        return;
    }
    if (c < 32 || c > 126) return;

    s->term_buf[s->term_cy][s->term_cx] = c;
    s->term_cx++;
    if (s->term_cx >= TERM_COLS) {
        s->term_cx = 0;
        s->term_cy++;
        if (s->term_cy >= TERM_ROWS) term_scroll_state(s);
    }
}

extern "C" void terminal_puts_state(struct TerminalState *s, const char *str) {
    while (*str) terminal_putchar_state(s, *str++);
}

// =====================================================================
// Dibujo
// =====================================================================
extern "C" void terminal_draw_in_rect(struct TerminalState *s,
                                       int x, int y, int w, int h) {
    if (!s) return;

    // Fondo
    gfx_fill_rect(x, y, w, h, TERM_BG);

    // Texto
    int text_x0 = x + 4;
    int text_y0 = y + 4;

    for (int row = 0; row < TERM_ROWS; row++) {
        for (int col = 0; col < TERM_COLS; col++) {
            char ch = s->term_buf[row][col];
            if (ch == 0 || ch == ' ') continue;

            int px = text_x0 + col * CHAR_W;
            int py = text_y0 + row * CHAR_H;
            if (px + CHAR_W > x + w) continue;   // clipping horizontal
            if (py + CHAR_H > y + h) continue;   // clipping vertical

            unsigned int color = TERM_FG;
            if (col == 0 && ch == '>') color = TERM_PROMPT;

            gfx_draw_char(px, py, ch, color, TERM_BG);
        }
    }

    // Cursor
    if (s->has_focus && s->cursor_visible) {
        int cx = text_x0 + s->term_cx * CHAR_W;
        int cy = text_y0 + s->term_cy * CHAR_H;
        if (cx + CHAR_W <= x + w && cy + CHAR_H <= y + h) {
            gfx_fill_rect(cx, cy, CHAR_W, CHAR_H, TERM_FG);
        }
    }
}

// =====================================================================
// Foco y parpadeo
// =====================================================================
extern "C" void terminal_set_focus_state(struct TerminalState *s, int focus) {
    if (s) s->has_focus = focus;
}

extern "C" void terminal_blink_state(struct TerminalState *s) {
    if (!s || !s->has_focus) return;
    unsigned int now = pit_get_ticks();
    if (now - s->cursor_last_tick >= CURSOR_BLINK_TICKS) {
        s->cursor_last_tick = now;
        s->cursor_visible = !s->cursor_visible;
    }
}
