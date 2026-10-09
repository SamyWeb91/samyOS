#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =====================================================================
// Tipos de widget
// =====================================================================
enum WidgetType {
    WIDGET_NONE     = 0,
    WIDGET_LABEL    = 1,
    WIDGET_BUTTON   = 2,
    WIDGET_INPUT    = 3,
    WIDGET_TERMINAL = 4,
};

// =====================================================================
// Callbacks
// =====================================================================
typedef void (*WidgetClickFn)(int widget_id, void *user_data);
typedef void (*WidgetKeyFn)(int widget_id, char c, void *user_data);

// =====================================================================
// Estructura Widget
// =====================================================================
#define WIDGET_TEXT_MAX   128
#define WIDGET_INPUT_MAX  256

struct Widget {
    int          type;
    int          id;
    int          visible;

    int          x, y, w, h;

    char         text[WIDGET_TEXT_MAX];
    uint32_t     fg, bg;

    int          hover;
    int          pressed;
    int          focused;

    char         input_buf[WIDGET_INPUT_MAX];
    int          input_len;
    int          input_max;
    int          cursor_pos;

    WidgetClickFn on_click;
    WidgetKeyFn   on_key;
    void         *user_data;

    void         *term_state;
};

// =====================================================================
// API de widgets
// =====================================================================
void widget_draw(struct Widget *w, int win_x, int win_y);
int  widget_hit(struct Widget *w, int win_x, int win_y, int mx, int my);
void widget_set_hover(struct Widget *w, int hover);
void widget_on_press(struct Widget *w);
void widget_on_release(struct Widget *w);
void widget_on_click(struct Widget *w);
void widget_on_key(struct Widget *w, char c);
void widget_init_basic(struct Widget *w, int type,
                        int x, int y, int width, int height);

#ifdef __cplusplus
}
#endif
