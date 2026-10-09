#include "widget.h"
#include "gfx.h"
#include "screen.h"
#include "terminal.h"
// =====================================================================
// Colores por defecto
// =====================================================================
#define COL_LABEL_FG       0x00EEEEEE
#define COL_LABEL_BG       0x00000000

#define COL_BUTTON_FG      0x00FFFFFF
#define COL_BUTTON_BG      0x00404080
#define COL_BUTTON_HOVER   0x006060A0
#define COL_BUTTON_PRESS   0x00303060

#define COL_INPUT_FG       0x00EEEEEE
#define COL_INPUT_BG       0x00181818
#define COL_INPUT_BORDER   0x00606060
#define COL_INPUT_FOCUS    0x00A0A0FF

#define CHAR_W  8
#define CHAR_H  16

// =====================================================================
// Helper: obtener color de fondo del boton segun estado
// =====================================================================
static uint32_t button_bg(struct Widget *w) {
    if (w->pressed) return COL_BUTTON_PRESS;
    if (w->hover)   return COL_BUTTON_HOVER;
    return COL_BUTTON_BG;
}

/// =====================================================================
// Dibujar un widget
//
// win_x, win_y: posicion absoluta de la esquina superior izquierda
//               del AREA DE CONTENIDO de la ventana.
// =====================================================================
extern "C" void widget_draw(struct Widget *w, int win_x, int win_y) {
    if (!w || !w->visible) return;

    int ax = win_x + w->x;
    int ay = win_y + w->y;

    switch (w->type) {
        case WIDGET_LABEL: {
            gfx_fill_rect(ax, ay, w->w, w->h, w->bg);
            gfx_draw_string(ax + 4, ay + 2, w->text, w->fg, w->bg);
            break;
        }

        case WIDGET_BUTTON: {
            gfx_fill_rect(ax, ay, w->w, w->h, button_bg(w));
            gfx_draw_rect(ax, ay, w->w, w->h, 0x00000000);

            // Centrar el texto
            int len = 0;
            while (w->text[len]) len++;
            int tw = len * CHAR_W;
            int tx = ax + (w->w - tw) / 2;
            int ty = ay + (w->h - CHAR_H) / 2;
            gfx_draw_string(tx, ty, w->text, w->fg, button_bg(w));
            break;
        }

        case WIDGET_INPUT: {
            // Fondo
            gfx_fill_rect(ax, ay, w->w, w->h, COL_INPUT_BG);

            // Borde (foco = azul claro)
            uint32_t border = w->focused ? COL_INPUT_FOCUS : COL_INPUT_BORDER;
            gfx_draw_rect(ax, ay, w->w, w->h, border);

            // Texto
            int inner_x = ax + 4;
            int inner_y = ay + (w->h - CHAR_H) / 2;

            for (int i = 0; i < w->input_len; i++) {
                gfx_draw_char(inner_x + i * CHAR_W, inner_y,
                              w->input_buf[i], COL_INPUT_FG, COL_INPUT_BG);
            }

            // Cursor
            if (w->focused) {
                int cx = inner_x + w->cursor_pos * CHAR_W;
                gfx_fill_rect(cx, inner_y, 2, CHAR_H, COL_INPUT_FG);
            }
            break;
        }

        case WIDGET_TERMINAL: {
            // El widget terminal guarda un TerminalState* en term_state.
            // terminal_draw_in_rect recibe ESE estado.
            struct TerminalState *ts = (struct TerminalState *)w->term_state;
            terminal_draw_in_rect(ts, ax, ay, w->w, w->h);
            break;
        }

        default:
            break;
    }
}

// =====================================================================
// Hit test: esta el punto (mx,my) dentro del widget?
//
// mx, my: coordenadas absolutas de pantalla.
// win_x, win_y: posicion absoluta del area de contenido de la ventana.
// =====================================================================
extern "C" int widget_hit(struct Widget *w, int win_x, int win_y,
                          int mx, int my) {
    if (!w || !w->visible) return 0;
    if (w->type == WIDGET_LABEL) return 0;   // labels no reciben clicks

    int ax = win_x + w->x;
    int ay = win_y + w->y;
    return (mx >= ax && mx < ax + w->w &&
            my >= ay && my < ay + w->h);
}

// =====================================================================
// Pasar el raton por encima (hover)
// =====================================================================
extern "C" void widget_set_hover(struct Widget *w, int hover) {
    if (!w) return;
    if (w->type != WIDGET_BUTTON) return;
    w->hover = hover;
}

// =====================================================================
// Click
// =====================================================================
extern "C" void widget_on_press(struct Widget *w) {
    if (!w) return;
    if (w->type == WIDGET_BUTTON) w->pressed = 1;
}

extern "C" void widget_on_release(struct Widget *w) {
    if (!w) return;
    if (w->type == WIDGET_BUTTON) w->pressed = 0;
}

extern "C" void widget_on_click(struct Widget *w) {
    if (!w || !w->on_click) return;
    w->on_click(w->id, w->user_data);
}

// =====================================================================
// Teclado
// =====================================================================
extern "C" void widget_on_key(struct Widget *w, char c) {
    if (!w) return;

    // Primero, si el widget es INPUT, actualizar su buffer
    if (w->type == WIDGET_INPUT) {
        if (c == '\b') {
            if (w->cursor_pos > 0) {
                // Borrar el caracter anterior al cursor
                for (int i = w->cursor_pos - 1; i < w->input_len - 1; i++) {
                    w->input_buf[i] = w->input_buf[i + 1];
                }
                w->input_len--;
                w->cursor_pos--;
            }
        } else if (c >= 32 && c < 127) {
            if (w->input_len < w->input_max - 1) {
                // Insertar en la posicion del cursor
                for (int i = w->input_len; i > w->cursor_pos; i--) {
                    w->input_buf[i] = w->input_buf[i - 1];
                }
                w->input_buf[w->cursor_pos] = c;
                w->input_len++;
                w->cursor_pos++;
            }
        }
        // Enter: llamar al callback (aunque no suele usarse en inputs)
    }

    // Notificar al callback del widget
    if (w->on_key) {
        w->on_key(w->id, c, w->user_data);
    }
}

// =====================================================================
// Init de un widget (comun)
// =====================================================================
extern "C" void widget_init_basic(struct Widget *w, int type,
                                   int x, int y, int width, int height) {
    w->type    = type;
    w->id      = -1;   // lo asigna window_add_*
    w->visible = 1;
    w->x = x;
    w->y = y;
    w->w = width;
    w->h = height;
    w->text[0] = 0;
    w->fg = COL_LABEL_FG;
    w->bg = COL_LABEL_BG;
    w->hover   = 0;
    w->pressed = 0;
    w->focused = 0;
    w->input_buf[0] = 0;
    w->input_len = 0;
    w->input_max = WIDGET_INPUT_MAX;
    w->cursor_pos = 0;
    w->on_click = 0;
    w->on_key   = 0;
    w->user_data = 0;
    w->term_state = 0;
}
