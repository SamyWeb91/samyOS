#include "window.h"
#include "gfx.h"
#include "screen.h"

// =====================================================================
// Array global de ventanas
// =====================================================================
static struct Window windows[MAX_WINDOWS];

// Z-order: windows[z_order[0]] es la de mas abajo,
// windows[z_order[MAX_WINDOWS-1]] es la de mas arriba.
static int z_order[MAX_WINDOWS];
static int window_count = 0;

// Id global para widgets (unico en todo el sistema)
static int next_widget_id = 1;

// =====================================================================
// Helper: recalcular z_order tras destruir una ventana
// =====================================================================
static void z_rebuild(void) {
    int k = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].id == i && windows[i].visible) {
            z_order[k++] = i;
        }
    }
    for (int i = k; i < MAX_WINDOWS; i++) {
        z_order[i] = -1;
    }
}

// =====================================================================
// Init (por si acaso, llamar al arrancar el desktop)
// =====================================================================
extern "C" void window_system_init(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        windows[i].id = -1;
        windows[i].visible = 0;
        windows[i].focused = 0;
        windows[i].widget_count = 0;
        windows[i].focused_widget = -1;
        windows[i].on_key = 0;
        windows[i].user_data = 0;
        z_order[i] = -1;
    }
    window_count = 0;
    next_widget_id = 1;
}

// =====================================================================
// Crear
// =====================================================================
extern "C" int window_create(int x, int y, int w, int h, const char *title) {
    if (w < 100 || h < 80) return -1;

    int idx = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].id == -1) { idx = i; break; }
    }
    if (idx < 0) return -1;

    struct Window *win = &windows[idx];
    win->id = idx;
    win->visible = 1;
    win->focused = 0;
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;

    int k = 0;
    while (title[k] && k < 63) { win->title[k] = title[k]; k++; }
    win->title[k] = 0;

    win->widget_count = 0;
    win->focused_widget = -1;
    win->on_key = 0;
    win->user_data = 0;

    window_count++;
    z_rebuild();
    window_raise(idx);

    return idx;
}

// =====================================================================
// Destruir
// =====================================================================
extern "C" void window_destroy(int id) {
    if (id < 0 || id >= MAX_WINDOWS) return;
    if (windows[id].id != id) return;

    windows[id].id = -1;
    windows[id].visible = 0;
    windows[id].focused = 0;
    windows[id].widget_count = 0;
    windows[id].focused_widget = -1;

    window_count--;
    z_rebuild();
}

extern "C" void window_show(int id) {
    if (id < 0 || id >= MAX_WINDOWS) return;
    if (windows[id].id != id) return;
    windows[id].visible = 1;
    z_rebuild();
}

extern "C" void window_hide(int id) {
    if (id < 0 || id >= MAX_WINDOWS) return;
    if (windows[id].id != id) return;
    windows[id].visible = 0;
    z_rebuild();
}

// =====================================================================
// Posicion y tamano
// =====================================================================
extern "C" void window_set_pos(int id, int x, int y) {
    if (id < 0 || id >= MAX_WINDOWS) return;
    if (windows[id].id != id) return;
    windows[id].x = x;
    windows[id].y = y;
}

extern "C" void window_get_rect(int id, int *x, int *y, int *w, int *h) {
    if (id < 0 || id >= MAX_WINDOWS) return;
    if (windows[id].id != id) return;
    if (x) *x = windows[id].x;
    if (y) *y = windows[id].y;
    if (w) *w = windows[id].w;
    if (h) *h = windows[id].h;
}

extern "C" int window_get_content_x(int id) {
    if (id < 0 || id >= MAX_WINDOWS) return 0;
    return windows[id].x + WIN_PADDING;
}

extern "C" int window_get_content_y(int id) {
    if (id < 0 || id >= MAX_WINDOWS) return 0;
    return windows[id].y + WIN_TITLEBAR_H + WIN_PADDING;
}

// =====================================================================
// Foco y z-order
// =====================================================================
extern "C" void window_set_focus(int id) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        windows[i].focused = (i == id && windows[i].id == i) ? 1 : 0;
    }
}

extern "C" int window_get_focused(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].id == i && windows[i].focused) return i;
    }
    return -1;
}

extern "C" void window_raise(int id) {
    if (id < 0 || id >= MAX_WINDOWS) return;
    if (windows[id].id != id) return;

    // Buscar el id en z_order
    int pos = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (z_order[i] == id) { pos = i; break; }
    }
    if (pos < 0) return;

    // Moverlo al final
    for (int i = pos; i < MAX_WINDOWS - 1; i++) {
        z_order[i] = z_order[i + 1];
    }
    z_order[MAX_WINDOWS - 1] = id;
}

// =====================================================================
// Anadir widgets
// =====================================================================
static struct Widget *alloc_widget(struct Window *win) {
    if (win->widget_count >= MAX_WIDGETS_PER_WINDOW) return 0;
    struct Widget *w = &win->widgets[win->widget_count++];
    return w;
}

extern "C" int window_add_label(int id, int x, int y, const char *text) {
    if (id < 0 || id >= MAX_WINDOWS || windows[id].id != id) return -1;
    struct Widget *w = alloc_widget(&windows[id]);
    if (!w) return -1;

    widget_init_basic(w, WIDGET_LABEL, x, y, 0, 18);
    w->id = next_widget_id++;

    int k = 0;
    while (text[k] && k < WIDGET_TEXT_MAX - 1) { w->text[k] = text[k]; k++; }
    w->text[k] = 0;

    // Ajustar ancho segun texto
    w->w = (k * 8) + 8;

    return w->id;
}

extern "C" int window_add_button(int id, int x, int y, int bw, int bh,
                                  const char *text, WidgetClickFn on_click,
                                  void *user_data) {
    if (id < 0 || id >= MAX_WINDOWS || windows[id].id != id) return -1;
    struct Widget *w = alloc_widget(&windows[id]);
    if (!w) return -1;

    widget_init_basic(w, WIDGET_BUTTON, x, y, bw, bh);
    w->id = next_widget_id++;
    w->on_click = on_click;
    w->user_data = user_data;

    int k = 0;
    while (text[k] && k < WIDGET_TEXT_MAX - 1) { w->text[k] = text[k]; k++; }
    w->text[k] = 0;

    return w->id;
}

extern "C" int window_add_input(int id, int x, int y, int iw, int ih,
                                 const char *initial_text) {
    if (id < 0 || id >= MAX_WINDOWS || windows[id].id != id) return -1;
    struct Widget *w = alloc_widget(&windows[id]);
    if (!w) return -1;

    widget_init_basic(w, WIDGET_INPUT, x, y, iw, ih);
    w->id = next_widget_id++;

    int k = 0;
    while (initial_text[k] && k < WIDGET_INPUT_MAX - 1) {
        w->input_buf[k] = initial_text[k];
        k++;
    }
    w->input_buf[k] = 0;
    w->input_len = k;
    w->cursor_pos = k;

    return w->id;
}

extern "C" int window_add_terminal(int id, int x, int y, int tw, int th,
                                    void *term_state) {
    if (id < 0 || id >= MAX_WINDOWS || windows[id].id != id) return -1;
    struct Widget *w = alloc_widget(&windows[id]);
    if (!w) return -1;

    widget_init_basic(w, WIDGET_TERMINAL, x, y, tw, th);
    w->id = next_widget_id++;
    w->term_state = term_state;

    return w->id;
}

// =====================================================================
// Dibujo
// =====================================================================
extern "C" void window_draw(int id) {
    if (id < 0 || id >= MAX_WINDOWS) return;
    struct Window *win = &windows[id];
    if (win->id != id || !win->visible) return;

    int wx = win->x;
    int wy = win->y;
    int ww = win->w;
    int wh = win->h;

    // Sombra
    gfx_fill_rect(wx + 6, wy + 6, ww, wh, WIN_COLOR_SHADOW);

    // Borde exterior
    gfx_fill_rect(wx - 1, wy - 1, ww + 2, wh + 2, WIN_COLOR_BORDER);

    // Barra de titulo
    uint32_t tb_color = win->focused ? 0x003060A0 : WIN_COLOR_TITLEBAR;
    gfx_fill_rect(wx, wy, ww, WIN_TITLEBAR_H, tb_color);

    // Titulo
    gfx_draw_string(wx + 8, wy + 6, win->title,
                    WIN_COLOR_TITLE_FG, tb_color);

    // Boton cerrar
    int bx = wx + ww - WIN_BTN_W - 6;
    int by = wy + (WIN_TITLEBAR_H - WIN_BTN_H) / 2;
    gfx_fill_rect(bx, by, WIN_BTN_W, WIN_BTN_H, WIN_COLOR_BTN_CLOSE);
    for (int i = 0; i < 14; i++) {
        gfx_putpixel(bx + 5 + i, by + 3 + i, WIN_COLOR_BTN_X);
        gfx_putpixel(bx + 5 + i, by + WIN_BTN_H - 4 - i, WIN_COLOR_BTN_X);
    }

    // Cuerpo (fondo)
    gfx_fill_rect(wx, wy + WIN_TITLEBAR_H, ww, wh - WIN_TITLEBAR_H,
                  WIN_COLOR_BODY);

    // Area de contenido
    int cx = wx + WIN_PADDING;
    int cy = wy + WIN_TITLEBAR_H + WIN_PADDING;

    // Dibujar widgets
    for (int i = 0; i < win->widget_count; i++) {
        widget_draw(&win->widgets[i], cx, cy);
    }

    // Borde
    gfx_draw_rect(wx, wy, ww, wh, WIN_COLOR_BORDER);
}

// =====================================================================
// Hit tests
// =====================================================================
extern "C" int window_point_in_close(int id, int mx, int my) {
    if (id < 0 || id >= MAX_WINDOWS || windows[id].id != id) return 0;
    struct Window *win = &windows[id];
    if (!win->visible) return 0;
    int bx = win->x + win->w - WIN_BTN_W - 6;
    int by = win->y + (WIN_TITLEBAR_H - WIN_BTN_H) / 2;
    return (mx >= bx && mx < bx + WIN_BTN_W &&
            my >= by && my < by + WIN_BTN_H);
}

extern "C" int window_point_in_titlebar(int id, int mx, int my) {
    if (id < 0 || id >= MAX_WINDOWS || windows[id].id != id) return 0;
    struct Window *win = &windows[id];
    if (!win->visible) return 0;
    if (window_point_in_close(id, mx, my)) return 0;
    return (mx >= win->x && mx < win->x + win->w &&
            my >= win->y && my < win->y + WIN_TITLEBAR_H);
}

extern "C" int window_point_in_body(int id, int mx, int my) {
    if (id < 0 || id >= MAX_WINDOWS || windows[id].id != id) return 0;
    struct Window *win = &windows[id];
    if (!win->visible) return 0;
    int by0 = win->y + WIN_TITLEBAR_H;
    int by1 = win->y + win->h;
    return (mx >= win->x && mx < win->x + win->w &&
            my >= by0 && my < by1);
}

// =====================================================================
// Mouse
// =====================================================================
extern "C" int window_handle_mouse_move(int id, int mx, int my) {
    if (id < 0 || id >= MAX_WINDOWS || windows[id].id != id) return 0;
    struct Window *win = &windows[id];
    if (!win->visible) return 0;

    int cx = win->x + WIN_PADDING;
    int cy = win->y + WIN_TITLEBAR_H + WIN_PADDING;

    int consumed = 0;
    for (int i = 0; i < win->widget_count; i++) {
        struct Widget *w = &win->widgets[i];
        if (w->type == WIDGET_BUTTON) {
            int h = widget_hit(w, cx, cy, mx, my);
            if (h && !w->hover) { w->hover = 1; consumed = 1; }
            if (!h && w->hover)  { w->hover = 0; consumed = 1; }
        }
    }
    return consumed;
}

extern "C" int window_handle_mouse_click(int id, int mx, int my) {
    if (id < 0 || id >= MAX_WINDOWS || windows[id].id != id) return 0;
    struct Window *win = &windows[id];
    if (!win->visible) return 0;

    int cx = win->x + WIN_PADDING;
    int cy = win->y + WIN_TITLEBAR_H + WIN_PADDING;

    for (int i = 0; i < win->widget_count; i++) {
        struct Widget *w = &win->widgets[i];
        if (widget_hit(w, cx, cy, mx, my)) {
            widget_on_press(w);
            // Inputs: dar foco
            if (w->type == WIDGET_INPUT) {
                win->focused_widget = i;
                w->focused = 1;
            } else {
                // Quitar foco de otros inputs
                for (int j = 0; j < win->widget_count; j++) {
                    if (win->widgets[j].type == WIDGET_INPUT)
                        win->widgets[j].focused = 0;
                }
                win->focused_widget = -1;
            }
            return 1;
        }
    }
    return 0;
}

extern "C" int window_handle_mouse_release(int id, int mx, int my) {
    if (id < 0 || id >= MAX_WINDOWS || windows[id].id != id) return 0;
    struct Window *win = &windows[id];
    if (!win->visible) return 0;

    int cx = win->x + WIN_PADDING;
    int cy = win->y + WIN_TITLEBAR_H + WIN_PADDING;

    int consumed = 0;
    for (int i = 0; i < win->widget_count; i++) {
        struct Widget *w = &win->widgets[i];
        if (w->type == WIDGET_BUTTON && w->pressed) {
            if (widget_hit(w, cx, cy, mx, my)) {
                widget_on_click(w);
            }
            widget_on_release(w);
            consumed = 1;
        }
    }
    return consumed;
}

// =====================================================================
// Teclado
// =====================================================================
extern "C" int window_handle_key(int id, char c) {
    if (id < 0 || id >= MAX_WINDOWS || windows[id].id != id) return 0;
    struct Window *win = &windows[id];
    if (!win->visible || !win->focused) return 0;

    // Si hay un widget con foco, darle la tecla
    if (win->focused_widget >= 0 &&
        win->focused_widget < win->widget_count) {
        widget_on_key(&win->widgets[win->focused_widget], c);
        return 1;
    }

    // Si no, dar la tecla a la ventana
    if (win->on_key) {
        win->on_key(id, c);
        return 1;
    }
    return 0;
}

// =====================================================================
// Buscar ventana bajo un punto (topmost)
// =====================================================================
extern "C" int window_find_at(int mx, int my) {
    for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
        int id = z_order[i];
        if (id < 0 || windows[id].id != id) continue;
        if (!windows[id].visible) continue;
        if (window_point_in_body(id, mx, my) ||
            window_point_in_titlebar(id, mx, my)) {
            return id;
        }
    }
    return -1;
}

// =====================================================================
// Acceso
// =====================================================================
extern "C" struct Window *window_get(int id) {
    if (id < 0 || id >= MAX_WINDOWS) return 0;
    if (windows[id].id != id) return 0;
    return &windows[id];
}

extern "C" int window_get_count(void) {
    return window_count;
}
