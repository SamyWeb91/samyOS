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

#define COLOR_WIN_TITLEBAR   0x001A3A5C
#define COLOR_WIN_TITLE_FG   0x00FFFFFF
#define COLOR_WIN_BODY       0x00E0E0E0
#define COLOR_WIN_BORDER     0x00000000
#define COLOR_WIN_SHADOW     0x00404040
#define COLOR_WIN_TEXT_FG    0x00000000

#define COLOR_BTN_CLOSE_BG    0x00C03030
#define COLOR_BTN_CLOSE_FG    0x00FFFFFF
#define COLOR_BTN_CLOSE_HOVER 0x00FF4040

// =====================================================================
// Dimensiones
// =====================================================================
#define TASKBAR_HEIGHT  28
#define CLOCK_RIGHT     10

#define CURSOR_W  8
#define CURSOR_H  12

#define TITLEBAR_H  24

#define BTN_CLOSE_W  16
#define BTN_CLOSE_H  16

// =====================================================================
// Bitmap del cursor
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

// =====================================================================
// Ventana única
// =====================================================================
static int  win_x = 200;
static int  win_y = 150;
static int  win_w = 400;
static int  win_h = 240;
static const char *win_title = "Ventana 1";
static const char *win_text  = "Hola Mundo";
static int  win_visible = 1;

static int dragging   = 0;
static int drag_off_x = 0;
static int drag_off_y = 0;

// =====================================================================
// Región sucia
// =====================================================================
static int dirty_x = 0;
static int dirty_y = 0;
static int dirty_w = 0;
static int dirty_h = 0;

static void dirty_reset(void) {
    dirty_w = 0;
    dirty_h = 0;
}

static void dirty_union(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;

    if (dirty_w <= 0 || dirty_h <= 0) {
        dirty_x = x; dirty_y = y;
        dirty_w = w; dirty_h = h;
        return;
    }

    int x1 = dirty_x;
    int y1 = dirty_y;
    int x2 = dirty_x + dirty_w;
    int y2 = dirty_y + dirty_h;

    if (x < x1) x1 = x;
    if (y < y1) y1 = y;
    if (x + w > x2) x2 = x + w;
    if (y + h > y2) y2 = y + h;

    dirty_x = x1;
    dirty_y = y1;
    dirty_w = x2 - x1;
    dirty_h = y2 - y1;
}

static void dirty_all(void) {
    dirty_x = 0;
    dirty_y = 0;
    dirty_w = gfx_width();
    dirty_h = gfx_height();
}

// =====================================================================
// Helpers
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

static void clock_rect(int *x, int *y, int *w, int *h) {
    int W = gfx_width();
    int H = gfx_height();
    *x = W - CLOCK_RIGHT - 8 * 8 - 4;
    *y = H - TASKBAR_HEIGHT + 1;
    *w = 8 * 8 + 8;
    *h = TASKBAR_HEIGHT - 1;
}

// =====================================================================
// Botón cerrar
// =====================================================================
static void btn_close_rect(int *bx, int *by) {
    *bx = win_x + win_w - BTN_CLOSE_W - 4;
    *by = win_y + (TITLEBAR_H - BTN_CLOSE_H) / 2;
}

static int point_in_btn_close(int mx, int my) {
    if (!win_visible) return 0;
    int bx, by;
    btn_close_rect(&bx, &by);
    return (mx >= bx && mx < bx + BTN_CLOSE_W &&
            my >= by && my < by + BTN_CLOSE_H);
}

static void btn_close_draw(int hover) {
    int bx, by;
    btn_close_rect(&bx, &by);

    uint32_t bg = hover ? COLOR_BTN_CLOSE_HOVER : COLOR_BTN_CLOSE_BG;
    gfx_fill_rect(bx, by, BTN_CLOSE_W, BTN_CLOSE_H, bg);

    for (int i = 0; i < BTN_CLOSE_W; i++) {
        gfx_putpixel(bx + i, by + i, COLOR_BTN_CLOSE_FG);
        gfx_putpixel(bx + i, by + BTN_CLOSE_H - 1 - i, COLOR_BTN_CLOSE_FG);
    }
}

// =====================================================================
// Ventana
// =====================================================================
static void window_rect(int *x, int *y, int *w, int *h) {
    *x = win_x;
    *y = win_y;
    *w = win_w + 4;   // incluye sombra
    *h = win_h + 4;
}

static void window_draw(int mx, int my) {
    if (!win_visible) return;

    gfx_fill_rect(win_x + 4, win_y + 4, win_w, win_h, COLOR_WIN_SHADOW);
    gfx_fill_rect(win_x, win_y, win_w, TITLEBAR_H, COLOR_WIN_TITLEBAR);
    gfx_draw_string(win_x + 8, win_y + 4, win_title,
                    COLOR_WIN_TITLE_FG, COLOR_WIN_TITLEBAR);

    gfx_fill_rect(win_x, win_y + TITLEBAR_H, win_w, win_h - TITLEBAR_H,
                  COLOR_WIN_BODY);

    gfx_draw_string(win_x + 16, win_y + TITLEBAR_H + 16, win_text,
                    COLOR_WIN_TEXT_FG, COLOR_WIN_BODY);

    gfx_draw_rect(win_x, win_y, win_w, win_h, COLOR_WIN_BORDER);

    btn_close_draw(point_in_btn_close(mx, my));
}

static int point_in_titlebar(int mx, int my) {
    if (!win_visible) return 0;
    if (point_in_btn_close(mx, my)) return 0;
    return (mx >= win_x && mx < win_x + win_w &&
            my >= win_y && my < win_y + TITLEBAR_H);
}

// =====================================================================
// Cursor
// =====================================================================
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

// =====================================================================
// Reconstrucción del fondo del escritorio en una región
// =====================================================================
// Reconstruye la escena (fondo, taskbar, reloj, ventana) en la región
// sucia EXACTA. Sin ampliar a toda la ventana.
static void redraw_region(int rx, int ry, int rw, int rh, int mx, int my) {
    int W = gfx_width();
    int H = gfx_height();

    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > W) rw = W - rx;
    if (ry + rh > H) rh = H - ry;
    if (rw <= 0 || rh <= 0) return;

    // 1. Fondo del escritorio SOLO en la intersección con la zona del escritorio
    int desk_h = H - TASKBAR_HEIGHT;
    if (ry < desk_h) {
        int y0 = ry;
        int h0 = rh;
        if (y0 + h0 > desk_h) h0 = desk_h - y0;
        if (h0 > 0) {
            gfx_fill_rect(rx, y0, rw, h0, COLOR_DESKTOP_BG);
        }
    }

    // 2. Etiquetas del escritorio (si la región toca la parte superior)
    if (ry < 60 && ry + rh > 20) {
        gfx_draw_string(20, 20, "MiniOS Desktop", 0x00FFFFFF, COLOR_DESKTOP_BG);
        gfx_draw_string(20, 40, "Pulsa ESC para volver a la shell.",
                        0x00FFFFFF, COLOR_DESKTOP_BG);
    }

    // 3. Taskbar (si la región toca la barra)
    if (ry + rh > H - TASKBAR_HEIGHT) {
        draw_taskbar();
        draw_clock();
    }

    // 4. Ventana: SOLO si la región intersecta con la ventana.
    //    Redibujamos la ventana completa, pero SOLO si hay intersección.
    //    El clipping de gfx_* la recorta al framebuffer, pero no a la región.
    //    Solución: guardar la parte de la ventana que ya estaba correcta y
    //    solo reescribir la ventana completa cuando la región la toca.
    //    Aquí "la toca" es lo que dispara el repintado de toda la ventana.
    if (win_visible) {
        int wx, wy, ww, wh;
        window_rect(&wx, &wy, &ww, &wh);
        int overlap = !(rx + rw <= wx || rx >= wx + ww ||
                        ry + rh <= wy || ry >= wy + wh);
        if (overlap) {
            // Redibujar la ventana completa (más fácil que clipar).
            window_draw(mx, my);
        }
    }
}

// =====================================================================
// Frame: aplica la región sucia, dibuja cursor, flip
// =====================================================================
static void apply_dirty(int mx, int my) {
    if (dirty_w <= 0 || dirty_h <= 0) return;

    int W = gfx_width();
    int H = gfx_height();

    int rx = dirty_x;
    int ry = dirty_y;
    int rw = dirty_w;
    int rh = dirty_h;
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > W) rw = W - rx;
    if (ry + rh > H) rh = H - ry;
    if (rw <= 0 || rh <= 0) { dirty_reset(); return; }

    // Reconstruir la escena en la región
    redraw_region(rx, ry, rw, rh, mx, my);

    // Dibujar el cursor encima (si toca la región)
    int overlap_cursor = !(rx + rw <= mx || rx >= mx + CURSOR_W ||
                           ry + rh <= my || ry >= my + CURSOR_H);
    if (overlap_cursor) {
        cursor_draw(mx, my);
    }

    // Copiar SOLO esa región al framebuffer real
    gfx_backbuffer_flip_region(rx, ry, rw, rh);

    dirty_reset();
}

// =====================================================================
// Escritorio
// =====================================================================
void desktop_run(void) {
    if (!gfx_ready()) {
        kprintf("desktop: gfx no disponible\n");
        return;
    }

    if (!gfx_backbuffer_init()) {
        kprintf("desktop: AVISO sin backbuffer (habra parpadeo)\n");
    }

    kprintf("desktop: inicializando raton...\n");
    int r = mouse_init();
    kprintf("desktop: mouse_init = %d\n", r);

    int W = gfx_width();
    int H = gfx_height();
    (void)W; (void)H;

    int mx = mouse_get_x();
    int my = mouse_get_y();

    // Primer frame completo
    dirty_all();
    apply_dirty(mx, my);

    unsigned int last_tick = pit_get_ticks();
    int last_left = 0;
    int last_hover_close = 0;

    while (1) {
        char c = keyboard_getchar();
        if (c == 27) break;

        unsigned int now = pit_get_ticks();
        if (now - last_tick >= 100) {
            last_tick = now;
            int cx, cy, cw, ch;
            clock_rect(&cx, &cy, &cw, &ch);
            dirty_union(cx, cy, cw, ch);
        }

        if (mouse_is_ready()) {
            int nmx = mouse_get_x();
            int nmy = mouse_get_y();

            // Movimiento del cursor: SOLO marcar cursor viejo y nuevo
            if (nmx != mx || nmy != my) {
                dirty_union(mx, my, CURSOR_W, CURSOR_H);
                mx = nmx;
                my = nmy;
                dirty_union(mx, my, CURSOR_W, CURSOR_H);
            }

            int now_left = mouse_left_pressed();
            int click = (now_left && !last_left);

            // Hover del botón cerrar
            int hover_close = point_in_btn_close(mx, my);
            if (hover_close != last_hover_close) {
                int bx, by;
                btn_close_rect(&bx, &by);
                dirty_union(bx, by, BTN_CLOSE_W, BTN_CLOSE_H);
                last_hover_close = hover_close;
            }

            if (click) {
                if (hover_close) {
                    if (win_visible) {
                        int wx, wy, ww, wh;
                        window_rect(&wx, &wy, &ww, &wh);
                        dirty_union(wx, wy, ww, wh);
                    }
                    win_visible = 0;
                    dragging = 0;
                    dirty_union(mx, my, CURSOR_W, CURSOR_H);
                } else if (point_in_titlebar(mx, my)) {
                    dragging   = 1;
                    drag_off_x = mx - win_x;
                    drag_off_y = my - win_y;
                }
            }

            if (!now_left && dragging) {
                dragging = 0;
            }

            if (dragging) {
                int nx = mx - drag_off_x;
                int ny = my - drag_off_y;
                if (nx != win_x || ny != win_y) {
                    int wx, wy, ww, wh;

                    window_rect(&wx, &wy, &ww, &wh);
                    dirty_union(wx, wy, ww, wh);

                    win_x = nx;
                    win_y = ny;

                    window_rect(&wx, &wy, &ww, &wh);
                    dirty_union(wx, wy, ww, wh);

                    dirty_union(mx, my, CURSOR_W, CURSOR_H);
                }
            }

            last_left = now_left;
        }

        if (dirty_w > 0 && dirty_h > 0) {
            apply_dirty(mx, my);
        }

        thread_yield();
    }

    gfx_backbuffer_free();
    screen_clear();
}
