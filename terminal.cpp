#include "terminal.h"
#include "gfx.h"
#include "pit.h"

// =====================================================================
// Estado de la terminal
// =====================================================================
static int  win_x = 100;
static int  win_y = 80;
static int  win_w = TERM_WIN_W;
static int  win_h = TERM_WIN_H;

static int  visible = 0;
static int  has_focus = 1;

// =====================================================================
// Buffer de texto
// =====================================================================
static char term_buf[TERM_ROWS][TERM_COLS];
static int  term_cx = 0;
static int  term_cy = 0;

static int          cursor_visible   = 1;
static unsigned int cursor_last_tick = 0;

#define CURSOR_BLINK_TICKS 50

// =====================================================================
// Colores
// =====================================================================
#define TERM_BG        0x000C0C0C
#define TERM_FG        0x00EEEEEE
#define TERM_PROMPT    0x0040FF40
#define TERM_TITLEBAR  0x00202020
#define TERM_TITLE_FG  0x00DDDDDD
#define TERM_BTN_CLOSE 0x00C03030
#define TERM_BTN_X     0x00FFFFFF
#define TERM_BORDER_IN 0x00505050

#define TERM_TITLE  "C:\\> MiniOS Terminal"

#define TITLEBAR_H  28
#define PADDING     8
#define BTN_W       24
#define BTN_H       20
#define CHAR_W      8
#define CHAR_H      16

// =====================================================================
// Helpers internos
// =====================================================================
static void term_scroll(void) {
    for (int row = 0; row < TERM_ROWS - 1; row++) {
        for (int col = 0; col < TERM_COLS; col++) {
            term_buf[row][col] = term_buf[row + 1][col];
        }
    }
    for (int col = 0; col < TERM_COLS; col++) {
        term_buf[TERM_ROWS - 1][col] = ' ';
    }
    if (term_cy > 0) term_cy--;
}

static void term_close_rect(int *bx, int *by) {
    *bx = win_x + win_w - BTN_W - 6;
    *by = win_y + (TITLEBAR_H - BTN_H) / 2;
}

// =====================================================================
// Init
// =====================================================================
void terminal_init(int x, int y, int w, int h) {
    win_x = x;
    win_y = y;
    win_w = w;
    win_h = h;

    terminal_clear();

    visible = 1;
    has_focus = 1;
    cursor_visible = 1;
    cursor_last_tick = pit_get_ticks();
}

void terminal_set_pos(int x, int y) {
    win_x = x;
    win_y = y;
}

void terminal_get_rect(int *x, int *y, int *w, int *h) {
    if (x) *x = win_x;
    if (y) *y = win_y;
    if (w) *w = win_w;
    if (h) *h = win_h;
}

// =====================================================================
// Buffer de texto
// =====================================================================
void terminal_clear(void) {
    for (int row = 0; row < TERM_ROWS; row++) {
        for (int col = 0; col < TERM_COLS; col++) {
            term_buf[row][col] = ' ';
        }
    }
    term_cx = 0;
    term_cy = 0;
    cursor_visible = 1;
    cursor_last_tick = pit_get_ticks();
}

void terminal_putchar(char c) {
    if (c == '\n') {
        term_cx = 0;
        term_cy++;
        if (term_cy >= TERM_ROWS) term_scroll();
        return;
    }
    if (c == '\r') {
        term_cx = 0;
        return;
    }
    if (c == '\b') {
        if (term_cx > 0) {
            term_cx--;
            term_buf[term_cy][term_cx] = ' ';
        }
        return;
    }

    if (c < 32 || c > 126) return;

    term_buf[term_cy][term_cx] = c;
    term_cx++;
    if (term_cx >= TERM_COLS) {
        term_cx = 0;
        term_cy++;
        if (term_cy >= TERM_ROWS) term_scroll();
    }
}

void terminal_puts(const char *s) {
    while (*s) terminal_putchar(*s++);
}

// =====================================================================
// Dibujo
// =====================================================================
void terminal_draw(void) {
    if (!visible) return;

    // Sombra
    gfx_fill_rect(win_x + 6, win_y + 6, win_w, win_h, 0x00000000);

    // Borde exterior
    gfx_fill_rect(win_x - 1, win_y - 1, win_w + 2, win_h + 2, 0x00000000);

    // Barra de titulo
    gfx_fill_rect(win_x, win_y, win_w, TITLEBAR_H, TERM_TITLEBAR);

    // Icono ">_"
    gfx_draw_string(win_x + 8, win_y + 6, ">_",
                    TERM_PROMPT, TERM_TITLEBAR);

    // Titulo
    gfx_draw_string(win_x + 30, win_y + 6, TERM_TITLE,
                    TERM_TITLE_FG, TERM_TITLEBAR);

    // Boton cerrar
    int bx, by;
    term_close_rect(&bx, &by);
    gfx_fill_rect(bx, by, BTN_W, BTN_H, TERM_BTN_CLOSE);
    for (int i = 0; i < 14; i++) {
        gfx_putpixel(bx + 5 + i, by + 3 + i, TERM_BTN_X);
        gfx_putpixel(bx + 5 + i, by + BTN_H - 4 - i, TERM_BTN_X);
    }

    // Cuerpo
    gfx_fill_rect(win_x, win_y + TITLEBAR_H, win_w, win_h - TITLEBAR_H,
                  TERM_BG);

    // Borde interior
    gfx_draw_rect(win_x + 2, win_y + TITLEBAR_H + 2,
                  win_w - 4, win_h - TITLEBAR_H - 4,
                  TERM_BORDER_IN);

    // Texto del buffer
    int text_x0 = win_x + PADDING;
    int text_y0 = win_y + TITLEBAR_H + PADDING;

    for (int row = 0; row < TERM_ROWS; row++) {
        for (int col = 0; col < TERM_COLS; col++) {
            char ch = term_buf[row][col];
            if (ch == 0 || ch == ' ') continue;

            int px = text_x0 + col * CHAR_W;
            int py = text_y0 + row * CHAR_H;

            unsigned int color = TERM_FG;
            if (col == 0 && ch == '>') color = TERM_PROMPT;

            gfx_draw_char(px, py, ch, color, TERM_BG);
        }
    }

    // Cursor parpadeante
    if (has_focus && cursor_visible) {
        int cx = text_x0 + term_cx * CHAR_W;
        int cy = text_y0 + term_cy * CHAR_H;
        gfx_fill_rect(cx, cy, CHAR_W, CHAR_H, TERM_FG);
    }
}

// =====================================================================
// Foco
// =====================================================================
void terminal_set_focus(int focus) { has_focus = focus; }
int  terminal_get_focus(void)      { return has_focus; }

// =====================================================================
// Parpadeo
// =====================================================================
void terminal_blink_tick(void) {
    if (!visible || !has_focus) return;

    unsigned int now = pit_get_ticks();
    if (now - cursor_last_tick >= CURSOR_BLINK_TICKS) {
        cursor_last_tick = now;
        cursor_visible = !cursor_visible;
    }
}

// =====================================================================
// Boton cerrar
// =====================================================================
int terminal_point_in_close(int mx, int my) {
    if (!visible) return 0;
    int bx, by;
    term_close_rect(&bx, &by);
    return (mx >= bx && mx < bx + BTN_W &&
            my >= by && my < by + BTN_H);
}

void terminal_close(void) {
    visible = 0;
}

int terminal_is_visible(void) {
    return visible;
}

// =====================================================================
// Barra de titulo (para arrastrar)
// =====================================================================
int terminal_point_in_titlebar(int mx, int my) {
    if (!visible) return 0;
    if (terminal_point_in_close(mx, my)) return 0;
    return (mx >= win_x && mx < win_x + win_w &&
            my >= win_y && my < win_y + TITLEBAR_H);
}

// =====================================================================
// Redireccion de teclado (ya NO se usa)
// =====================================================================
int terminal_handle_key(char c) {
    (void)c;
    return 0;
}
