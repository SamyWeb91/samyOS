#include "desktop.h"
#include "gfx.h"
#include "screen.h"
#include "rtc.h"
#include "pit.h"
#include "keyboard.h"
#include "thread.h"
#include "mouse.h"
#include "win.h"
#include "shell.h"      /* la shell REAL, con todos los comandos */

// =====================================================================
// Colores
// =====================================================================
#define COLOR_DESKTOP_BG     0x0087CEEB
#define COLOR_TASKBAR_BG     0x001A3A5C
#define COLOR_TASKBAR_TEXT   0x00FFFFFF
#define COLOR_TASKBAR_BORDER 0x00000000
#define COLOR_CURSOR_FILL    0x00FFFFFF
#define COLOR_CURSOR_BORDER  0x00000000

#define TASKBAR_HEIGHT  28
#define CLOCK_RIGHT     10
#define CURSOR_W        8
#define CURSOR_H        12

// =====================================================================
// Cursor
// =====================================================================
static const unsigned char cursor_bitmap[CURSOR_H][CURSOR_W] = {
    {2,0,0,0,0,0,0,0},{2,2,0,0,0,0,0,0},{2,1,2,0,0,0,0,0},
    {2,1,1,2,0,0,0,0},{2,1,1,1,2,0,0,0},{2,1,1,1,1,2,0,0},
    {2,1,1,1,1,1,2,0},{2,1,1,1,1,1,1,2},{2,1,1,1,2,2,2,2},
    {2,1,2,1,2,0,0,0},{2,2,0,2,1,2,0,0},{0,0,0,2,2,2,0,0},
};

// =====================================================================
// Ventana shell
// =====================================================================
static int shell_wid = -1;
static int shell_x = 60;
static int shell_y = 60;
static int shell_w = 640;
static int shell_h = 400;

// =====================================================================
// Callbacks para screen.cpp
// =====================================================================
extern "C" void desktop_terminal_putchar(char c) {
    if (shell_wid < 0) return;
    win_sys_write(shell_wid, &c, 1);
}

extern "C" void desktop_terminal_clear(void) {
    if (shell_wid < 0) return;
    win_sys_clear(shell_wid);
}

// =====================================================================
// Región sucia
// =====================================================================
static int dirty_x = 0, dirty_y = 0, dirty_w = 0, dirty_h = 0;

static void dirty_reset(void) { dirty_w = 0; dirty_h = 0; }

static void dirty_union(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (dirty_w <= 0 || dirty_h <= 0) {
        dirty_x = x; dirty_y = y; dirty_w = w; dirty_h = h;
        return;
    }
    int x1 = dirty_x, y1 = dirty_y;
    int x2 = dirty_x + dirty_w, y2 = dirty_y + dirty_h;
    if (x < x1) x1 = x;
    if (y < y1) y1 = y;
    if (x + w > x2) x2 = x + w;
    if (y + h > y2) y2 = y + h;
    dirty_x = x1; dirty_y = y1;
    dirty_w = x2 - x1; dirty_h = y2 - y1;
}

static void dirty_all(void) {
    dirty_x = 0; dirty_y = 0;
    dirty_w = gfx_width(); dirty_h = gfx_height();
}

static void dirty_shell(void) {
    if (shell_wid < 0) return;
    int x, y, w, h;
    if (win_get_rect(shell_wid, &x, &y, &w, &h) == 0)
        dirty_union(x - 8, y - 8, w + 16, h + 16);
}

// =====================================================================
// Helpers de dibujo
// =====================================================================
static void draw_taskbar(void) {
    int W = gfx_width(), H = gfx_height();
    gfx_fill_rect(0, H - TASKBAR_HEIGHT, W, TASKBAR_HEIGHT, COLOR_TASKBAR_BG);
    gfx_fill_rect(0, H - TASKBAR_HEIGHT, W, 1, COLOR_TASKBAR_BORDER);
}

static void draw_clock(void) {
    int W = gfx_width(), H = gfx_height();
    RTCTime t; rtc_read(&t);
    char buf[9];
    buf[0] = '0' + (t.hour / 10);   buf[1] = '0' + (t.hour % 10);   buf[2] = ':';
    buf[3] = '0' + (t.minute / 10); buf[4] = '0' + (t.minute % 10); buf[5] = ':';
    buf[6] = '0' + (t.second / 10); buf[7] = '0' + (t.second % 10); buf[8] = 0;
    int x = W - CLOCK_RIGHT - 8 * 8;
    int y = H - TASKBAR_HEIGHT + (TASKBAR_HEIGHT - 16) / 2;
    gfx_fill_rect(x - 4, H - TASKBAR_HEIGHT + 1,
                  8 * 8 + 8, TASKBAR_HEIGHT - 1, COLOR_TASKBAR_BG);
    gfx_draw_string(x, y, buf, COLOR_TASKBAR_TEXT, COLOR_TASKBAR_BG);
}

static void clock_rect(int *x, int *y, int *w, int *h) {
    int W = gfx_width(), H = gfx_height();
    *x = W - CLOCK_RIGHT - 8 * 8 - 4;
    *y = H - TASKBAR_HEIGHT + 1;
    *w = 8 * 8 + 8;
    *h = TASKBAR_HEIGHT - 1;
}

static void cursor_draw(int x, int y) {
    int W = gfx_width(), H = gfx_height();
    for (int j = 0; j < CURSOR_H; j++) {
        for (int i = 0; i < CURSOR_W; i++) {
            unsigned char v = cursor_bitmap[j][i];
            if (v == 0) continue;
            int px = x + i, py = y + j;
            if (px < 0 || px >= W || py < 0 || py >= H) continue;
            unsigned int color = (v == 2) ? COLOR_CURSOR_BORDER : COLOR_CURSOR_FILL;
            gfx_putpixel(px, py, color);
        }
    }
}

// =====================================================================
// Redibujo
// =====================================================================
static void redraw_region(int rx, int ry, int rw, int rh) {
    int W = gfx_width(), H = gfx_height();
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > W) rw = W - rx;
    if (ry + rh > H) rh = H - ry;
    if (rw <= 0 || rh <= 0) return;

    // 1. Fondo del escritorio
    int desk_h = H - TASKBAR_HEIGHT;
    if (ry < desk_h) {
        int y0 = ry, h0 = rh;
        if (y0 + h0 > desk_h) h0 = desk_h - y0;
        if (h0 > 0) gfx_fill_rect(rx, y0, rw, h0, COLOR_DESKTOP_BG);
    }

    // 2. Taskbar + reloj
    if (ry + rh > H - TASKBAR_HEIGHT) {
        draw_taskbar();
        draw_clock();
    }

    // 3. Ventanas (API win_*)
    win_redraw_all();
}

static void apply_dirty(int mx, int my) {
    if (dirty_w <= 0 || dirty_h <= 0) return;
    int W = gfx_width(), H = gfx_height();
    int rx = dirty_x, ry = dirty_y, rw = dirty_w, rh = dirty_h;
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > W) rw = W - rx;
    if (ry + rh > H) rh = H - ry;
    if (rw <= 0 || rh <= 0) { dirty_reset(); return; }

    redraw_region(rx, ry, rw, rh);

    int overlap_cursor = !(rx + rw <= mx || rx >= mx + CURSOR_W ||
                           ry + rh <= my || ry >= my + CURSOR_H);
    if (overlap_cursor) cursor_draw(mx, my);

    gfx_backbuffer_flip_region(rx, ry, rw, rh);
    dirty_reset();
}

// =====================================================================
// Escritorio principal
// =====================================================================
void desktop_run(void) {
    if (!gfx_ready()) {
        kprintf("desktop: gfx no disponible\n");
        return;
    }
    if (!gfx_backbuffer_init())
        kprintf("desktop: AVISO sin backbuffer\n");

    int r = mouse_init();
    kprintf("desktop: mouse_init = %d\n", r);

    int mx = mouse_get_x(), my = mouse_get_y();
    int last_left = 0;

    // 1. Crear la ventana shell con la API win_*
    shell_wid = win_sys_create(shell_x, shell_y, shell_w, shell_h,
                                "C:\\> MiniOS Shell");
    if (shell_wid < 0) {
        kprintf("desktop: no se pudo crear la ventana shell\n");
        return;
    }
    kprintf("desktop: shell window id=%d\n", shell_wid);

    // 2. Redirigir screen_putchar -> desktop_terminal_putchar
    screen_set_terminal_mode(1);

    // 3. Arrancar la shell REAL: imprime "> " en la ventana
    shell_init();

    // 4. Primera pintura completa
    dirty_all();
    apply_dirty(mx, my);

    unsigned int last_tick = pit_get_ticks();

    while (1) {
        // -------------------------------------------------------------
        // Teclado -> shell real
        // -------------------------------------------------------------
        char c = keyboard_getchar();
        if (c) {
            shell_handle_key(c);
            dirty_shell();
        }

        // -------------------------------------------------------------
        // Reloj
        // -------------------------------------------------------------
        unsigned int now = pit_get_ticks();
        if (now - last_tick >= 100) {
            last_tick = now;
            int cx, cy, cw, ch;
            clock_rect(&cx, &cy, &cw, &ch);
            dirty_union(cx, cy, cw, ch);
        }

        // -------------------------------------------------------------
        // Ratón
        // -------------------------------------------------------------
        if (mouse_is_ready()) {
            int nmx = mouse_get_x();
            int nmy = mouse_get_y();

            if (nmx != mx || nmy != my) {
                dirty_union(mx, my, CURSOR_W, CURSOR_H);
                mx = nmx; my = nmy;
                dirty_union(mx, my, CURSOR_W, CURSOR_H);
            }

            int now_left = mouse_left_pressed();
            int click = (now_left && !last_left);

            if (click) {
                int old_focus = win_get_focused();
                int fx = 0, fy = 0, fw = 0, fh = 0;
                if (old_focus >= 0)
                    win_get_rect(old_focus, &fx, &fy, &fw, &fh);

                win_handle_click(mx, my, 1);

                // Si se cerró la ventana que tenía foco, limpiar su zona
                if (old_focus >= 0 && win_get_focused() != old_focus) {
                    dirty_union(fx - 8, fy - 8, fw + 16, fh + 16);
                    if (old_focus == shell_wid) shell_wid = -1;
                }

                dirty_union(mx, my, CURSOR_W, CURSOR_H);
            }

            last_left = now_left;
        }

        if (dirty_w > 0 && dirty_h > 0)
            apply_dirty(mx, my);

        thread_yield();
    }
}
