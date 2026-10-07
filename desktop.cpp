#include "desktop.h"
#include "gfx.h"
#include "screen.h"
#include "rtc.h"
#include "pit.h"
#include "keyboard.h"
#include "thread.h"
#include "mouse.h"

// =====================================================================
// Colores
// =====================================================================
#define COLOR_DESKTOP_BG     0x0087CEEB
#define COLOR_TASKBAR_BG     0x001A3A5C
#define COLOR_TASKBAR_TEXT   0x00FFFFFF
#define COLOR_TASKBAR_BORDER 0x00000000

#define COLOR_CURSOR_FILL    0x00FFFFFF
#define COLOR_CURSOR_BORDER  0x00000000

#define COLOR_WIN_TITLEBAR   0x001A3A5C   // azul oscuro (igual que taskbar)
#define COLOR_WIN_TITLE_FG   0x00FFFFFF   // blanco
#define COLOR_WIN_BODY       0x00E0E0E0   // gris claro
#define COLOR_WIN_BORDER     0x00000000   // negro
#define COLOR_WIN_SHADOW     0x00404040   // gris oscuro
#define COLOR_WIN_TEXT_FG    0x00000000   // negro (texto dentro)

// =====================================================================
// Dimensiones
// =====================================================================
#define TASKBAR_HEIGHT  28
#define CLOCK_RIGHT     10

#define CURSOR_W  8
#define CURSOR_H  12

#define TITLEBAR_H  24

// =====================================================================
// Bitmap del cursor: 0=transparente, 1=blanco, 2=negro
// =====================================================================
static const unsigned char cursor_bitmap[CURSOR_H][CURSOR_W] = {
    {2,0,0,0,0,0,0,0},
    {2,2,0,0,0,0,0,0},
    {2,1,2,0,0,0,0,0},
    {2,1,1,2,0,0,0,0},
    {2,1,1,1,2,0,0,0},
    {2,1,1,1,1,2,0,0},
    {2,1,1,1,1,1,2,0},
    {2,1,1,1,1,1,1,2},
    {2,1,1,1,2,2,2,2},
    {2,1,2,1,2,0,0,0},
    {2,2,0,2,1,2,0,0},
    {0,0,0,2,2,2,0,0},
};

static unsigned int cursor_bg[CURSOR_H][CURSOR_W];
static int cursor_x = -1;
static int cursor_y = -1;

// =====================================================================
// Ventana única
// =====================================================================
static int  win_x = 200;
static int  win_y = 150;
static int  win_w = 400;
static int  win_h = 240;
static const char *win_title = "Ventana 1";
static const char *win_text  = "Hola Mundo";

// Estado del arrastre
static int dragging   = 0;
static int drag_off_x = 0;
static int drag_off_y = 0;

// =====================================================================
// Helpers de escritorio
// =====================================================================
static void draw_taskbar(void) {
    int W = gfx_width();
    int H = gfx_height();
    gfx_fill_rect(0, H - TASKBAR_HEIGHT, W, TASKBAR_HEIGHT, COLOR_TASKBAR_BG);
    gfx_fill_rect(0, H - TASKBAR_HEIGHT, W, 1, COLOR_TASKBAR_BORDER);
}

static void draw_clock(void) {
    int W = gfx_width();
    int H = gfx_height();

    RTCTime t;
    rtc_read(&t);

    char buf[9];
    buf[0] = '0' + (t.hour / 10);
    buf[1] = '0' + (t.hour % 10);
    buf[2] = ':';
    buf[3] = '0' + (t.minute / 10);
    buf[4] = '0' + (t.minute % 10);
    buf[5] = ':';
    buf[6] = '0' + (t.second / 10);
    buf[7] = '0' + (t.second % 10);
    buf[8] = 0;

    int x = W - CLOCK_RIGHT - 8 * 8;
    int y = H - TASKBAR_HEIGHT + (TASKBAR_HEIGHT - 16) / 2;

    gfx_fill_rect(x - 4, H - TASKBAR_HEIGHT + 1,
                  8 * 8 + 8, TASKBAR_HEIGHT - 1, COLOR_TASKBAR_BG);

    gfx_draw_string(x, y, buf, COLOR_TASKBAR_TEXT, COLOR_TASKBAR_BG);
}

static void draw_desktop_bg(void) {
    int W = gfx_width();
    int H = gfx_height();

    gfx_fill_rect(0, 0, W, H - TASKBAR_HEIGHT, COLOR_DESKTOP_BG);
    draw_taskbar();
    draw_clock();

    gfx_draw_string(20, 20, "MiniOS Desktop", 0x00FFFFFF, COLOR_DESKTOP_BG);
    gfx_draw_string(20, 40, "Pulsa ESC para volver a la shell.",
                    0x00FFFFFF, COLOR_DESKTOP_BG);
}

// =====================================================================
// Cursor
// =====================================================================
static void cursor_save_bg(int x, int y) {
    int W = gfx_width();
    int H = gfx_height();

    for (int j = 0; j < CURSOR_H; j++) {
        for (int i = 0; i < CURSOR_W; i++) {
            int px = x + i;
            int py = y + j;
            if (px >= 0 && px < W && py >= 0 && py < H) {
                cursor_bg[j][i] = gfx_getpixel(px, py);
            } else {
                cursor_bg[j][i] = COLOR_DESKTOP_BG;
            }
        }
    }
}

static void cursor_restore_bg(int x, int y) {
    int W = gfx_width();
    int H = gfx_height();

    for (int j = 0; j < CURSOR_H; j++) {
        for (int i = 0; i < CURSOR_W; i++) {
            int px = x + i;
            int py = y + j;
            if (px >= 0 && px < W && py >= 0 && py < H) {
                gfx_putpixel(px, py, cursor_bg[j][i]);
            }
        }
    }
}

static void cursor_draw(int x, int y) {
    int W = gfx_width();
    int H = gfx_height();

    for (int j = 0; j < CURSOR_H; j++) {
        for (int i = 0; i < CURSOR_W; i++) {
            unsigned char v = cursor_bitmap[j][i];
            if (v == 0) continue;

            int px = x + i;
            int py = y + j;
            if (px < 0 || px >= W || py < 0 || py >= H) continue;

            unsigned int color = (v == 2) ? COLOR_CURSOR_BORDER
                                          : COLOR_CURSOR_FILL;
            gfx_putpixel(px, py, color);
        }
    }
}

static void cursor_move(int nx, int ny) {
    int W = gfx_width();
    int H = gfx_height();

    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx > W - CURSOR_W) nx = W - CURSOR_W;
    if (ny > H - CURSOR_H) ny = H - CURSOR_H;

    if (nx == cursor_x && ny == cursor_y) return;

    if (cursor_x >= 0 && cursor_y >= 0) {
        cursor_restore_bg(cursor_x, cursor_y);
    }

    cursor_save_bg(nx, ny);
    cursor_draw(nx, ny);

    cursor_x = nx;
    cursor_y = ny;
}

// =====================================================================
// Ventana
// =====================================================================
static void window_draw(void) {
    // Sombra desplazada 4 px
    gfx_fill_rect(win_x + 4, win_y + 4, win_w, win_h, COLOR_WIN_SHADOW);

    // Barra de título
    gfx_fill_rect(win_x, win_y, win_w, TITLEBAR_H, COLOR_WIN_TITLEBAR);
    gfx_draw_string(win_x + 8, win_y + 4, win_title,
                    COLOR_WIN_TITLE_FG, COLOR_WIN_TITLEBAR);

    // Cuerpo
    gfx_fill_rect(win_x, win_y + TITLEBAR_H, win_w, win_h - TITLEBAR_H,
                  COLOR_WIN_BODY);

    // Texto dentro de la ventana
    gfx_draw_string(win_x + 16, win_y + TITLEBAR_H + 16, win_text,
                    COLOR_WIN_TEXT_FG, COLOR_WIN_BODY);

    // Borde
    gfx_draw_rect(win_x, win_y, win_w, win_h, COLOR_WIN_BORDER);
}

// Punto (mx,my) dentro de la barra de título
static int point_in_titlebar(int mx, int my) {
    return (mx >= win_x && mx < win_x + win_w &&
            my >= win_y && my < win_y + TITLEBAR_H);
}

// Redibuja todo: fondo + ventana + cursor (usado al arrastrar)
static void redraw_all(int mx, int my) {
    draw_desktop_bg();
    window_draw();

    cursor_x = -1;
    cursor_y = -1;
    cursor_save_bg(mx, my);
    cursor_draw(mx, my);
    cursor_x = mx;
    cursor_y = my;
}

// =====================================================================
// Escritorio
// =====================================================================
void desktop_run(void) {
    if (!gfx_ready()) {
        kprintf("desktop: gfx no disponible\n");
        return;
    }

    kprintf("desktop: inicializando raton...\n");
    int r = mouse_init();
    kprintf("desktop: mouse_init = %d\n", r);

    int W = gfx_width();
    int H = gfx_height();

    // Posición inicial del ratón
    int mx = mouse_get_x();
    int my = mouse_get_y();

    // Dibujo inicial
    redraw_all(mx, my);

    unsigned int last_tick = pit_get_ticks();
    int last_left = 0;

    while (1) {
        char c = keyboard_getchar();
        if (c == 27) break;   // ESC

        // Reloj cada segundo
        unsigned int now = pit_get_ticks();
        if (now - last_tick >= 100) {
            last_tick = now;
            draw_clock();
        }

        if (mouse_is_ready()) {
            mx = mouse_get_x();
            my = mouse_get_y();

            int now_left = mouse_left_pressed();
            int click = (now_left && !last_left);

            // Iniciar arrastre si el clic fue en la barra de título
            if (click && point_in_titlebar(mx, my)) {
                dragging   = 1;
                drag_off_x = mx - win_x;
                drag_off_y = my - win_y;
            }

            // Soltar botón
            if (!now_left && dragging) {
                dragging = 0;
            }

            if (dragging) {
                int nx = mx - drag_off_x;
                int ny = my - drag_off_y;

                if (nx != win_x || ny != win_y) {
                    win_x = nx;
                    win_y = ny;
                    redraw_all(mx, my);
                }
            } else {
                cursor_move(mx, my);
            }

            last_left = now_left;
        }

        thread_yield();
    }

    screen_clear();
}
