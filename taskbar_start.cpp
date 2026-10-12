//code
// taskbar_start.cpp
#include "taskbar_start.h"
#include "gfx.h"
#include "screen.h"
#include "win.h"

/* =====================================================================
 * Colores
 * ===================================================================== */
#define COLOR_START_BTN       0x002E5F8C
#define COLOR_START_BTN_HOV   0x00417CB0
#define COLOR_START_BTN_TXT   0x00FFFFFF
#define COLOR_START_DOT       0x0066D9FF

#define COLOR_MENU_BG         0x00F4F6FB
#define COLOR_MENU_BORDER     0x0094A3B8
#define COLOR_MENU_TEXT       0x001F2937
#define COLOR_MENU_HOVER      0x00DBEAFE
#define COLOR_MENU_ACCENT     0x003B82F6
#define COLOR_MENU_HEADER     0x002E5F8C
#define COLOR_MENU_HEADER_TX  0x00FFFFFF
#define COLOR_MENU_SEP        0x00CBD5E1

/* =====================================================================
 * Geometria
 * ===================================================================== */
#define TASKBAR_HEIGHT  28
#define START_BTN_X     4
#define START_BTN_W     84
#define START_BTN_H     (TASKBAR_HEIGHT - 6)
#define MENU_W          180
#define MENU_HEADER_H   20
#define MENU_ITEM_H     22
#define MENU_PAD        4

/* =====================================================================
 * Items del menu
 * ===================================================================== */
typedef struct {
    const char *label;
    const char *icon;
    int action;
} StartItem;

enum {
    ACT_NONE = 0,
    ACT_TERMINAL,
    ACT_ABOUT,
    ACT_REBOOT,
    ACT_CLOSE_ALL,
};

static const StartItem start_items[] = {
    { "Terminal",     ">_", ACT_TERMINAL  },
    { "Acerca de",    "i ", ACT_ABOUT     },
    { "Reiniciar",    "R ", ACT_REBOOT    },
    { "Cerrar todo",  "x ", ACT_CLOSE_ALL },
};
static const int START_ITEM_COUNT = (int)(sizeof(start_items)/sizeof(start_items[0]));

/* =====================================================================
 * Estado
 * ===================================================================== */
static int start_open       = 0;
static int start_btn_hover  = 0;
static int start_hover_item = -1;

/* Callback que desktop.cpp registra para ejecutar acciones */
static void (*start_action_cb)(int action) = 0;

/* =====================================================================
 * Geometria — funciones
 * ===================================================================== */
static void start_btn_rect(int *x, int *y, int *w, int *h) {
    *x = START_BTN_X;
    *y = gfx_height() - TASKBAR_HEIGHT + 3;
    *w = START_BTN_W;
    *h = START_BTN_H;
}

static void start_menu_rect(int *x, int *y, int *w, int *h) {
    int total_h = MENU_HEADER_H + MENU_PAD * 2 + START_ITEM_COUNT * MENU_ITEM_H;
    *x = START_BTN_X;
    *y = gfx_height() - TASKBAR_HEIGHT - total_h - 4;
    *w = MENU_W;
    *h = total_h;
}

/* =====================================================================
 * Dibujo
 * ===================================================================== */
static void draw_start_button(void) {
    int bx, by, bw, bh;
    start_btn_rect(&bx, &by, &bw, &bh);

    unsigned int bg = start_btn_hover ? COLOR_START_BTN_HOV : COLOR_START_BTN;
    gfx_fill_rect(bx, by, bw, bh, bg);

    /* Cuadrado decorativo a la izquierda */
    gfx_fill_rect(bx + 4, by + 4, bh - 8, bh - 8, COLOR_START_DOT);

    /* Texto "Inicio" */
    gfx_draw_string(bx + 20, by + (bh - 8) / 2,
                    "Inicio", COLOR_START_BTN_TXT, bg);
}

static void draw_start_menu(void) {
    if (!start_open) return;

    int mx, my, mw, mh;
    start_menu_rect(&mx, &my, &mw, &mh);

    /* Cuerpo */
    gfx_fill_rect(mx, my, mw, mh, COLOR_MENU_BG);
    gfx_draw_rect(mx, my, mw, mh, COLOR_MENU_BORDER);

    /* Cabecera */
    gfx_fill_rect(mx + 1, my + 1, mw - 2, MENU_HEADER_H - 1, COLOR_MENU_HEADER);
    gfx_draw_string(mx + 8, my + (MENU_HEADER_H - 8) / 2,
                    "MiniSO", COLOR_MENU_HEADER_TX, COLOR_MENU_HEADER);

    /* Separador */
    gfx_fill_rect(mx + 1, my + MENU_HEADER_H, mw - 2, 1, COLOR_MENU_SEP);

    /* Items */
    int item_y0 = my + MENU_HEADER_H + MENU_PAD;
    for (int i = 0; i < START_ITEM_COUNT; i++) {
        int iy = item_y0 + i * MENU_ITEM_H;
        int ix = mx + 1;
        int iw = mw - 2;

        unsigned int bg = (i == start_hover_item) ? COLOR_MENU_HOVER : COLOR_MENU_BG;
        gfx_fill_rect(ix, iy, iw, MENU_ITEM_H, bg);

        if (i == start_hover_item)
            gfx_fill_rect(ix, iy, 3, MENU_ITEM_H, COLOR_MENU_ACCENT);

        gfx_draw_string(ix + 10, iy + (MENU_ITEM_H - 8) / 2,
                        start_items[i].icon, COLOR_MENU_ACCENT, bg);
        gfx_draw_string(ix + 32, iy + (MENU_ITEM_H - 8) / 2,
                        start_items[i].label, COLOR_MENU_TEXT, bg);
    }
}

/* =====================================================================
 * API publica
 * ===================================================================== */
extern "C" void taskbar_start_init(void (*action_cb)(int)) {
    start_action_cb = action_cb;
    start_open = 0;
    start_btn_hover = 0;
    start_hover_item = -1;
}

extern "C" void taskbar_start_draw(void) {
    draw_start_button();
    draw_start_menu();
}

extern "C" int taskbar_start_is_open(void) {
    return start_open;
}

extern "C" void taskbar_start_close(void) {
    if (start_open) {
        start_open = 0;
        start_hover_item = -1;
    }
}

extern "C" int taskbar_start_hover(int mx, int my) {
    int changed = 0;

    /* Hover boton */
    int bx, by, bw, bh;
    start_btn_rect(&bx, &by, &bw, &bh);
    int hov = (mx >= bx && mx < bx + bw && my >= by && my < by + bh) ? 1 : 0;
    if (hov != start_btn_hover) {
        start_btn_hover = hov;
        changed = 1;
    }

    /* Hover item del menu */
    int new_hover = -1;
    if (start_open) {
        int mmx, mmy, mmw, mmh;
        start_menu_rect(&mmx, &mmy, &mmw, &mmh);
        if (mx >= mmx && mx < mmx + mmw && my >= mmy && my < mmy + mmh) {
            int item_y0 = mmy + MENU_HEADER_H + MENU_PAD;
            for (int i = 0; i < START_ITEM_COUNT; i++) {
                int iy = item_y0 + i * MENU_ITEM_H;
                if (my >= iy && my < iy + MENU_ITEM_H) {
                    new_hover = i;
                    break;
                }
            }
        }
    }
    if (new_hover != start_hover_item) {
        start_hover_item = new_hover;
        changed = 1;
    }

    return changed;
}

extern "C" int taskbar_start_click(int mx, int my) {
    /* Si el menu esta abierto, comprobar primero si se pulsa un item */
    if (start_open) {
        int mmx, mmy, mmw, mmh;
        start_menu_rect(&mmx, &mmy, &mmw, &mmh);

        if (mx >= mmx && mx < mmx + mmw && my >= mmy && my < mmy + mmh) {
            /* Cabecera o padding: no hacer nada, consumir click */
            int item_y0 = mmy + MENU_HEADER_H + MENU_PAD;
            for (int i = 0; i < START_ITEM_COUNT; i++) {
                int iy = item_y0 + i * MENU_ITEM_H;
                if (my >= iy && my < iy + MENU_ITEM_H) {
                    int action = start_items[i].action;
                    start_open = 0;
                    start_hover_item = -1;
                    if (start_action_cb) start_action_cb(action);
                    return 1;
                }
            }
            return 1; /* Click dentro del menu pero no en un item */
        }

        /* Click fuera del menu: cerrar y NO consumir (dejar pasar a la ventana) */
        start_open = 0;
        start_hover_item = -1;
        return 0;
    }

    /* Menu cerrado: comprobar click en el boton "Inicio" */
    int bx, by, bw, bh;
    start_btn_rect(&bx, &by, &bw, &bh);
    if (mx >= bx && mx < bx + bw && my >= by && my < by + bh) {
        start_open = 1;
        start_hover_item = -1;
        return 1;
    }

    return 0;
}
