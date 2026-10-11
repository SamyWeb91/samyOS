#include "win.h"
#include "gfx.h"
#include "screen.h"

#define W_BG        0x00E8E8E8
#define W_TITLE_BG  0x003060A0
#define W_TITLE_FG  0x00FFFFFF
#define W_BORDER    0x00202020
#define W_TEXT_FG   0x00101010
#define W_TEXT_BG   0x00E8E8E8
#define W_SHADOW    0x00404040
#define W_CLOSE_BG  0x00C03030
#define W_CLOSE_FG  0x00FFFFFF

#define CHAR_W      8
#define CHAR_H      16
#define TITLE_H     20

struct Win32 {
    int  used;
    int  id;
    int  x, y, w, h;
    char title[WIN32_TITLE_MAX];
    char buf[WIN32_ROWS][WIN32_COLS];
    int  cx, cy;
    /* un rectángulo opcional persistente */
    int  has_rect;
    int  rx, ry, rw, rh;
    uint32_t rcolor;
};

static struct Win32 wins[WIN32_MAX];
static int next_id = 1;
static int focused_id = -1;

extern "C" void win_init(void) {
    for (int i = 0; i < WIN32_MAX; i++) {
        wins[i].used = 0;
        wins[i].id   = -1;
        wins[i].has_rect = 0;
    }
    next_id = 1;
    focused_id = -1;
}

static struct Win32 *find(int id) {
    for (int i = 0; i < WIN32_MAX; i++)
        if (wins[i].used && wins[i].id == id) return &wins[i];
    return 0;
}

static void draw_one(struct Win32 *v) {
    /* sombra */
    gfx_fill_rect(v->x + 4, v->y + 4, v->w, v->h, W_SHADOW);

    /* cuerpo */
    gfx_fill_rect(v->x, v->y + TITLE_H, v->w, v->h - TITLE_H, W_BG);

    /* barra de título */
    gfx_fill_rect(v->x, v->y, v->w, TITLE_H, W_TITLE_BG);
    gfx_draw_string(v->x + 6, v->y + 2, v->title, W_TITLE_FG, W_TITLE_BG);

    /* botón cerrar (X) */
    int bx = v->x + v->w - 18;
    int by = v->y + 3;
    gfx_fill_rect(bx, by, 14, 14, W_CLOSE_BG);
    for (int i = 0; i < 10; i++) {
        gfx_putpixel(bx + 2 + i, by + 2 + i, W_CLOSE_FG);
        gfx_putpixel(bx + 2 + i, by + 11 - i, W_CLOSE_FG);
    }

    /* borde */
    gfx_draw_rect(v->x, v->y, v->w, v->h, W_BORDER);

    /* texto del buffer */
    int tx = v->x + 6;
    int ty = v->y + TITLE_H + 4;
    for (int r = 0; r < WIN32_ROWS; r++) {
        for (int c = 0; c < WIN32_COLS; c++) {
            char ch = v->buf[r][c];
            if (ch == 0 || ch == ' ') continue;
            int px = tx + c * CHAR_W;
            int py = ty + r * CHAR_H;
            if (px + CHAR_W > v->x + v->w - 2) continue;
            if (py + CHAR_H > v->y + v->h - 2) continue;
            gfx_draw_char(px, py, ch, W_TEXT_FG, W_TEXT_BG);
        }
    }

    /* rectángulo persistente */
    if (v->has_rect) {
        gfx_fill_rect(v->x + v->rx,
                      v->y + TITLE_H + v->ry,
                      v->rw, v->rh, v->rcolor);
    }
}

extern "C" void win_redraw_all(void) {
    for (int i = 0; i < WIN32_MAX; i++)
        if (wins[i].used) draw_one(&wins[i]);
}

extern "C" int win_sys_create(int x, int y, int w, int h, const char *title) {
    if (w < 80 || h < 40) return -1;
    int idx = -1;
    for (int i = 0; i < WIN32_MAX; i++)
        if (!wins[i].used) { idx = i; break; }
    if (idx < 0) return -1;

    struct Win32 *v = &wins[idx];
    v->used = 1;
    v->id   = next_id++;
    v->x = x; v->y = y; v->w = w; v->h = h;
    v->cx = 0; v->cy = 0;
    v->has_rect = 0;

    int k = 0;
    while (title && title[k] && k < WIN32_TITLE_MAX - 1) {
        v->title[k] = title[k]; k++;
    }
    v->title[k] = 0;

    for (int r = 0; r < WIN32_ROWS; r++)
        for (int c = 0; c < WIN32_COLS; c++)
            v->buf[r][c] = ' ';

    focused_id = v->id;
    return v->id;
}

extern "C" int win_sys_destroy(int id) {
    struct Win32 *v = find(id);
    if (!v) return -1;
    v->used = 0;
    v->id   = -1;
    v->has_rect = 0;
    if (focused_id == id) focused_id = -1;
    return 0;
}

extern "C" int win_sys_clear(int id) {
    struct Win32 *v = find(id);
    if (!v) return -1;
    for (int r = 0; r < WIN32_ROWS; r++)
        for (int c = 0; c < WIN32_COLS; c++)
            v->buf[r][c] = ' ';
    v->cx = 0; v->cy = 0;
    return 0;
}

extern "C" int win_sys_write(int id, const char *str, int len) {
    struct Win32 *v = find(id);
    if (!v) return -1;

    for (int i = 0; i < len; i++) {
        char c = str[i];
        if (c == '\n') {
            v->cx = 0;
            v->cy++;
            if (v->cy >= WIN32_ROWS) {
                for (int r = 0; r < WIN32_ROWS - 1; r++)
                    for (int cc = 0; cc < WIN32_COLS; cc++)
                        v->buf[r][cc] = v->buf[r + 1][cc];
                for (int cc = 0; cc < WIN32_COLS; cc++)
                    v->buf[WIN32_ROWS - 1][cc] = ' ';
                v->cy = WIN32_ROWS - 1;
            }
        } else if (c == '\b') {
            if (v->cx > 0) {
                v->cx--;
                v->buf[v->cy][v->cx] = ' ';
            }
        } else if (c >= 32 && c < 127) {
            v->buf[v->cy][v->cx] = c;
            v->cx++;
            if (v->cx >= WIN32_COLS) {
                v->cx = 0;
                v->cy++;
                if (v->cy >= WIN32_ROWS) {
                    for (int r = 0; r < WIN32_ROWS - 1; r++)
                        for (int cc = 0; cc < WIN32_COLS; cc++)
                            v->buf[r][cc] = v->buf[r + 1][cc];
                    for (int cc = 0; cc < WIN32_COLS; cc++)
                        v->buf[WIN32_ROWS - 1][cc] = ' ';
                    v->cy = WIN32_ROWS - 1;
                }
            }
        }
    }
    return len;
}

extern "C" int win_sys_rect(int id, int x, int y, int w, int h, uint32_t color) {
    struct Win32 *v = find(id);
    if (!v) return -1;
    v->has_rect = 1;
    v->rx = x; v->ry = y; v->rw = w; v->rh = h;
    v->rcolor = color;
    return 0;
}

extern "C" int win_sys_title(int id, const char *title) {
    struct Win32 *v = find(id);
    if (!v) return -1;
    int k = 0;
    while (title && title[k] && k < WIN32_TITLE_MAX - 1) {
        v->title[k] = title[k]; k++;
    }
    v->title[k] = 0;
    return 0;
}

/* ---------- helpers para el desktop ---------- */

static int hit_test(int mx, int my) {
    for (int i = WIN32_MAX - 1; i >= 0; i--) {
        if (!wins[i].used) continue;
        struct Win32 *v = &wins[i];
        if (mx >= v->x && mx < v->x + v->w &&
            my >= v->y && my < v->y + v->h)
            return v->id;
    }
    return -1;
}

static int is_close_button(struct Win32 *v, int mx, int my) {
    int bx = v->x + v->w - 18;
    int by = v->y + 3;
    return (mx >= bx && mx < bx + 14 && my >= by && my < by + 14);
}

extern "C" void win_handle_click(int mx, int my, int button) {
    if (button != 1) return;

    int id = hit_test(mx, my);
    if (id < 0) {
        focused_id = -1;
        return;
    }

    struct Win32 *v = find(id);
    if (!v) return;

    focused_id = id;

    if (is_close_button(v, mx, my)) {
        win_sys_destroy(id);
    }
}

extern "C" int win_get_focused(void) {
    return focused_id;
}

extern "C" void win_set_focused(int id) {
    focused_id = id;
}

extern "C" int win_get_rect(int id, int *x, int *y, int *w, int *h) {
    struct Win32 *v = find(id);
    if (!v) return -1;
    *x = v->x; *y = v->y; *w = v->w; *h = v->h;
    return 0;
}

/* =====================================================================
 * NUEVAS: foco y movimiento para el compositor
 * ===================================================================== */

extern "C" int win_focus(int id) {
    int idx = -1;
    for (int i = 0; i < WIN32_MAX; i++) {
        if (wins[i].used && wins[i].id == id) { idx = i; break; }
    }
    if (idx < 0) return -1;

    /* Mover la ventana al final del array (se dibuja encima) */
    if (idx != WIN32_MAX - 1) {
        struct Win32 tmp = wins[idx];
        for (int i = idx; i < WIN32_MAX - 1; i++) {
            wins[i] = wins[i + 1];
        }
        wins[WIN32_MAX - 1] = tmp;
    }

    focused_id = id;
    return 0;
}

extern "C" int win_move(int id, int x, int y) {
    struct Win32 *v = find(id);
    if (!v) return -1;
    v->x = x;
    v->y = y;
    return 0;
}
