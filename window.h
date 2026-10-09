#pragma once

#include <stdint.h>
#include "widget.h"

#ifdef __cplusplus
extern "C" {
#endif

// =====================================================================
// Limites
// =====================================================================
#define MAX_WINDOWS             8
#define MAX_WIDGETS_PER_WINDOW  16

// =====================================================================
// Colores de ventana
// =====================================================================
#define WIN_COLOR_TITLEBAR       0x00202020
#define WIN_COLOR_TITLE_FG       0x00DDDDDD
#define WIN_COLOR_BODY           0x00101010
#define WIN_COLOR_BORDER         0x00000000
#define WIN_COLOR_SHADOW         0x00000000
#define WIN_COLOR_BTN_CLOSE      0x00C03030
#define WIN_COLOR_BTN_X          0x00FFFFFF

// =====================================================================
// Estructura Window
// =====================================================================
struct Window {
    int      id;                // 0..MAX_WINDOWS-1, o -1 si libre
    int      visible;
    int      focused;

    // Posicion en pantalla (esquina sup-izq de la barra de titulo)
    int      x, y, w, h;

    // Titulo
    char     title[64];

    // Widgets
    struct Widget widgets[MAX_WIDGETS_PER_WINDOW];
    int      widget_count;
    int      focused_widget;    // indice del widget con foco (-1 = ninguno)

    // Callback de teclado a nivel de ventana
    void (*on_key)(int window_id, char c);
    void  *user_data;
};

// =====================================================================
// Dimensiones
// =====================================================================
#define WIN_TITLEBAR_H   28
#define WIN_BTN_W        24
#define WIN_BTN_H        20
#define WIN_PADDING      6     // margen entre el borde y el area de contenido

// =====================================================================
// Ciclo de vida
// =====================================================================
void window_system_init(void);            // ← NUEVO

int  window_create(int x, int y, int w, int h, const char *title);
void window_destroy(int id);
void window_show(int id);
void window_hide(int id);

// =====================================================================
// Posicion y tamano
// =====================================================================
void window_set_pos(int id, int x, int y);
void window_get_rect(int id, int *x, int *y, int *w, int *h);
int  window_get_content_x(int id);   // x del area de contenido
int  window_get_content_y(int id);   // y del area de contenido

// =====================================================================
// Foco
// =====================================================================
void window_set_focus(int id);
int  window_get_focused(void);
void window_raise(int id);   // traer al frente

// =====================================================================
// Widgets
// =====================================================================
int  window_add_label(int id, int x, int y, const char *text);
int  window_add_button(int id, int x, int y, int w, int h,
                       const char *text, WidgetClickFn on_click,
                       void *user_data);
int  window_add_input(int id, int x, int y, int w, int h,
                      const char *initial_text);
int  window_add_terminal(int id, int x, int y, int w, int h, void *term_state);

// =====================================================================
// Dibujo
// =====================================================================
void window_draw(int id);

// =====================================================================
// Interaccion (llamadas desde el desktop)
// =====================================================================
int  window_point_in_close(int id, int mx, int my);
int  window_point_in_titlebar(int id, int mx, int my);
int  window_point_in_body(int id, int mx, int my);

// Hover y clicks. Devuelven 1 si el evento fue consumido.
int  window_handle_mouse_move(int id, int mx, int my);
int  window_handle_mouse_click(int id, int mx, int my);   // press
int  window_handle_mouse_release(int id, int mx, int my); // release
int  window_handle_key(int id, char c);

// =====================================================================
// Acceso directo (para el desktop)
// =====================================================================
struct Window *window_get(int id);
int window_get_count(void);
int window_find_at(int mx, int my);   // ventana bajo el punto (topmost)

#ifdef __cplusplus
}
#endif
