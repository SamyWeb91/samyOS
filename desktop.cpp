#include "desktop.h"
#include "gfx.h"
#include "screen.h"
#include "rtc.h"
#include "pit.h"
#include "keyboard.h"
#include "thread.h"
#include "mouse.h"
#include "terminal.h"
#include "shell.h"

// =====================================================================
// Colores
// =====================================================================
#define COLOR_DESKTOP_BG     0x0087CEEB
#define COLOR_TASKBAR_BG     0x001A3A5C
#define COLOR_TASKBAR_TEXT   0x00FFFFFF
#define COLOR_TASKBAR_BORDER 0x00000000

#define COLOR_CURSOR_FILL    0x00FFFFFF
#define COLOR_CURSOR_BORDER  0x00000000

// =====================================================================
// Dimensiones
// =====================================================================
#define TASKBAR_HEIGHT  28
#define CLOCK_RIGHT     10

#define CURSOR_W  8
#define CURSOR_H  12

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
// Estado de la terminal (arrastrable)
// =====================================================================
static int  term_x = 100;
static int  term_y = 80;
static int  term_w = TERM_WIN_W;
static int  term_h = TERM_WIN_H;
static int  term_dragging   = 0;
static int  term_drag_off_x = 0;
static int  term_drag_off_y = 0;

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
// Cursor del ratón
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
// Reconstrucción del escritorio en una región sucia
// =====================================================================
static void redraw_region(int rx, int ry, int rw, int rh, int mx, int my) {
    int W = gfx_width();
    int H = gfx_height();

    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > W) rw = W - rx;
    if (ry + rh > H) rh = H - ry;
    if (rw <= 0 || rh <= 0) return;

    // 1. Fondo del escritorio
    int desk_h = H - TASKBAR_HEIGHT;
    if (ry < desk_h) {
        int y0 = ry;
        int h0 = rh;
        if (y0 + h0 > desk_h) h0 = desk_h - y0;
        if (h0 > 0) {
            gfx_fill_rect(rx, y0, rw, h0, COLOR_DESKTOP_BG);
        }
    }

    // 2. Etiquetas del escritorio
    if (ry < 60 && ry + rh > 20) {
        gfx_draw_string(20, 20, "MiniOS Desktop", 0x00FFFFFF, COLOR_DESKTOP_BG);
        gfx_draw_string(20, 40, "Escribe comandos en la terminal.",
                        0x00FFFFFF, COLOR_DESKTOP_BG);
    }

    // 3. Taskbar
    if (ry + rh > H - TASKBAR_HEIGHT) {
        draw_taskbar();
        draw_clock();
    }

    // 4. Terminal
    if (terminal_is_visible()) {
        int tx, ty, tw, th;
        terminal_get_rect(&tx, &ty, &tw, &th);
        int overlap = !(rx + rw <= tx || rx >= tx + tw ||
                        ry + rh <= ty || ry >= ty + th);
        if (overlap) {
            terminal_draw();
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

    redraw_region(rx, ry, rw, rh, mx, my);

    int overlap_cursor = !(rx + rw <= mx || rx >= mx + CURSOR_W ||
                           ry + rh <= my || ry >= my + CURSOR_H);
    if (overlap_cursor) {
        cursor_draw(mx, my);
    }

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

    // =========================================================
    // 1. Activar redireccion a la terminal
    //    (a partir de aqui, screen_putchar -> terminal_putchar)
    // =========================================================
    screen_set_terminal_mode(1);

    // 2. Inicializar la terminal grafica
    terminal_init(term_x, term_y, term_w, term_h);
    terminal_set_focus(1);

    // 3. Arrancar la shell: imprime "> " en la terminal
    shell_init();

    // 4. Primer frame completo
    dirty_all();
    apply_dirty(mx, my);

    unsigned int last_tick = pit_get_ticks();
    int last_left = 0;

    while (1) {
        // -------------------------------------------------------------
        // Teclado: va a la SHELL, que escribe con screen_putchar,
        // que redirige a terminal_putchar.
        // -------------------------------------------------------------
        char c = keyboard_getchar();
        if (c) {
            shell_handle_key(c);

            int tx, ty, tw, th;
            terminal_get_rect(&tx, &ty, &tw, &th);
            dirty_union(tx - 1, ty - 1, tw + 2, th + 2);
        }

        // -------------------------------------------------------------
        // Parpadeo del cursor de la terminal + reloj
        // -------------------------------------------------------------
        unsigned int now = pit_get_ticks();
        if (now - last_tick >= 100) {
            last_tick = now;

            int cx, cy, cw, ch;
            clock_rect(&cx, &cy, &cw, &ch);
            dirty_union(cx, cy, cw, ch);

            if (terminal_is_visible() && terminal_get_focus()) {
                int tx, ty, tw, th;
                terminal_get_rect(&tx, &ty, &tw, &th);
                dirty_union(tx - 1, ty - 1, tw + 2, th + 2);
            }
        }
        terminal_blink_tick();

        // -------------------------------------------------------------
        // Ratón
        // -------------------------------------------------------------
        if (mouse_is_ready()) {
            int nmx = mouse_get_x();
            int nmy = mouse_get_y();

            if (nmx != mx || nmy != my) {
                dirty_union(mx, my, CURSOR_W, CURSOR_H);
                mx = nmx;
                my = nmy;
                dirty_union(mx, my, CURSOR_W, CURSOR_H);
            }

            int now_left = mouse_left_pressed();
            int click = (now_left && !last_left);

            if (click) {
                if (terminal_point_in_close(mx, my)) {
                    int tx, ty, tw, th;
                    terminal_get_rect(&tx, &ty, &tw, &th);
                    dirty_union(tx - 1, ty - 1, tw + 2, th + 2);
                    terminal_close();
                }
                else if (terminal_point_in_titlebar(mx, my)) {
                    term_dragging   = 1;
                    term_drag_off_x = mx - term_x;
                    term_drag_off_y = my - term_y;
                    terminal_set_focus(1);
                }
            }

            if (!now_left && term_dragging) {
                term_dragging = 0;
            }

            if (term_dragging) {
                int nx = mx - term_drag_off_x;
                int ny = my - term_drag_off_y;
                if (nx != term_x || ny != term_y) {
                    int tx, ty, tw, th;
                    terminal_get_rect(&tx, &ty, &tw, &th);
                    dirty_union(tx - 1, ty - 1, tw + 2, th + 2);

                    term_x = nx;
                    term_y = ny;
                    terminal_set_pos(nx, ny);

                    terminal_get_rect(&tx, &ty, &tw, &th);
                    dirty_union(tx - 1, ty - 1, tw + 2, th + 2);
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

    screen_set_terminal_mode(0);
    gfx_backbuffer_free();
}
