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
#include "window.h"
#include "widget.h"

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
// Estado global
// =====================================================================
static struct TerminalState g_term_state;
static int g_term_window_id = -1;
static int drag_window_id = -1;
static int drag_off_x = 0;
static int drag_off_y = 0;

// =====================================================================
// Región sucia
// =====================================================================
static int dirty_x = 0, dirty_y = 0, dirty_w = 0, dirty_h = 0;

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
    dirty_x = 0;
    dirty_y = 0;
    dirty_w = gfx_width();
    dirty_h = gfx_height();
}

static void dirty_window(int win_id) {
    if (win_id < 0) return;
    int x, y, w, h;
    window_get_rect(win_id, &x, &y, &w, &h);
    dirty_union(x - 2, y - 2, w + 6, h + 6);
}

static void dirty_cursor(int mx, int my) {
    dirty_union(mx - 1, my - 1, CURSOR_W + 2, CURSOR_H + 2);
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

static void cursor_draw(int x, int y) {
    int W = gfx_width();
    int H = gfx_height();
    for (int j = 0; j < CURSOR_H; j++) {
        for (int i = 0; i < CURSOR_W; i++) {
            unsigned char v = cursor_bitmap[j][i];
            if (v == 0) continue;
            int px = x + i, py = y + j;
            if (px < 0 || px >= W || py < 0 || py >= H) continue;
            unsigned int color = (v == 2) ? COLOR_CURSOR_BORDER
                                          : COLOR_CURSOR_FILL;
            gfx_putpixel(px, py, color);
        }
    }
}

// =====================================================================
// Redibujo de región (sin cursor)
// =====================================================================
static void redraw_region(int rx, int ry, int rw, int rh) {
    int W = gfx_width();
    int H = gfx_height();

    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > W) rw = W - rx;
    if (ry + rh > H) rh = H - ry;
    if (rw <= 0 || rh <= 0) return;

    // 1. Fondo
    int desk_h = H - TASKBAR_HEIGHT;
    if (ry < desk_h) {
        int h0 = rh;
        if (ry + h0 > desk_h) h0 = desk_h - ry;
        if (h0 > 0)
            gfx_fill_rect(rx, ry, rw, h0, COLOR_DESKTOP_BG);
    }

    // 2. Etiquetas
    if (ry < 60 && ry + rh > 20) {
        gfx_draw_string(20, 20, "MiniOS Desktop",
                        0x00FFFFFF, COLOR_DESKTOP_BG);
        gfx_draw_string(20, 40, "Escribe comandos en la terminal.",
                        0x00FFFFFF, COLOR_DESKTOP_BG);
    }

    // 3. Taskbar
    if (ry + rh > H - TASKBAR_HEIGHT) {
        draw_taskbar();
        draw_clock();
    }

    // 4. Ventanas
    for (int i = 0; i < MAX_WINDOWS; i++) {
        struct Window *win = window_get(i);
        if (!win || !win->visible) continue;

        int wx = win->x, wy = win->y, ww = win->w, wh = win->h;
        int overlap = !(rx + rw <= wx || rx >= wx + ww ||
                        ry + rh <= wy || ry >= wy + wh);
        if (overlap)
            window_draw(i);
    }
}

// =====================================================================
// Frame
// =====================================================================
static void apply_dirty(int mx, int my) {
    if (dirty_w <= 0 || dirty_h <= 0) return;

    int W = gfx_width();
    int H = gfx_height();

    // Siempre incluir el cursor en la región a actualizar
    dirty_cursor(mx, my);

    int rx = dirty_x, ry = dirty_y, rw = dirty_w, rh = dirty_h;
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > W) rw = W - rx;
    if (ry + rh > H) rh = H - ry;
    if (rw <= 0 || rh <= 0) {
        dirty_reset();
        return;
    }

    // Componer escena en backbuffer
    redraw_region(rx, ry, rw, rh);

    // Cursor SIEMPRE encima de todo
    cursor_draw(mx, my);

    // Un solo blit al framebuffer real
    if (gfx_backbuffer_active())
        gfx_backbuffer_flip_region(rx, ry, rw, rh);

    dirty_reset();
}

// =====================================================================
// Callbacks para screen.cpp
//
// IMPORTANTE: aqui marcamos el cursor como sucio. Si no, cuando
// kprintf escribe y redibuja la ventana, el cursor del raton
// desaparece debajo del redibujado.
// =====================================================================
void desktop_terminal_putchar(char c) {
    terminal_putchar_state(&g_term_state, c);

    // Redibujar la ventana terminal
    dirty_window(g_term_window_id);

    // Y el cursor (que esta encima y hay que volver a pintarlo)
    extern int mouse_get_x(void);
    extern int mouse_get_y(void);
    dirty_cursor(mouse_get_x(), mouse_get_y());
}

void desktop_terminal_clear(void) {
    terminal_clear_state(&g_term_state);
    dirty_window(g_term_window_id);
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
        kprintf("desktop: AVISO sin backbuffer\n");
    }

    int r = mouse_init();
    kprintf("desktop: mouse_init = %d\n", r);

    int mx = mouse_get_x();
    int my = mouse_get_y();

    window_system_init();

    g_term_window_id = window_create(100, 80, 700, 450,
                                      "C:\\> MiniOS Terminal");
    terminal_init_state(&g_term_state);

    window_add_terminal(g_term_window_id,
                        0, 0,
                        700 - WIN_PADDING * 2,
                        450 - WIN_TITLEBAR_H - WIN_PADDING * 2,
                        &g_term_state);
    window_set_focus(g_term_window_id);

    // kprintf / screen_* -> buffer del terminal
    screen_set_terminal_mode(1);

    shell_init();   // escribe "> " en el terminal

    dirty_all();
    apply_dirty(mx, my);

    unsigned int last_tick = pit_get_ticks();
    int last_left = 0;

    while (1) {
        // ---------------- Teclado ----------------
        char c = keyboard_getchar();
        if (c) {
            int focused = window_get_focused();

            // El foco va directo a la shell: la shell hace el eco
            // (screen_putchar -> desktop_terminal_putchar -> buffer).
            if (focused == g_term_window_id || focused < 0) {
                shell_handle_key(c);
            } else {
                int consumed = window_handle_key(focused, c);
                if (!consumed)
                    shell_handle_key(c);
            }

            // Nota: NO hace falta marcar dirty aqui.
            // desktop_terminal_putchar ya marca dirty_window
            // y dirty_cursor por cada caracter.
        }

        // ---------------- Reloj + blink ----------------
        unsigned int now = pit_get_ticks();
        if (now - last_tick >= 50) {
            last_tick = now;

            int H = gfx_height();
            int W = gfx_width();
            dirty_union(0, H - TASKBAR_HEIGHT, W, TASKBAR_HEIGHT);

            terminal_blink_state(&g_term_state);
            dirty_window(g_term_window_id);
        }

        // ---------------- Ratón ----------------
        if (mouse_is_ready()) {
            int nmx = mouse_get_x();
            int nmy = mouse_get_y();

            // Clamp por si el driver entrega fuera de pantalla
            if (nmx < 0) nmx = 0;
            if (nmy < 0) nmy = 0;
            if (nmx >= gfx_width())  nmx = gfx_width() - 1;
            if (nmy >= gfx_height()) nmy = gfx_height() - 1;

            if (nmx != mx || nmy != my) {
                dirty_cursor(mx, my);   // zona vieja
                mx = nmx;
                my = nmy;
                dirty_cursor(mx, my);   // zona nueva

                for (int i = 0; i < MAX_WINDOWS; i++) {
                    if (window_handle_mouse_move(i, mx, my))
                        dirty_window(i);
                }
            }

            int now_left = mouse_left_pressed();
            int click = (now_left && !last_left);

            if (click) {
                int win_id = window_find_at(mx, my);
                if (win_id >= 0) {
                    if (window_point_in_close(win_id, mx, my)) {
                        window_hide(win_id);
                        dirty_all();
                    } else if (window_point_in_titlebar(win_id, mx, my)) {
                        drag_window_id = win_id;
                        int wx, wy;
                        window_get_rect(win_id, &wx, &wy, 0, 0);
                        drag_off_x = mx - wx;
                        drag_off_y = my - wy;
                        window_set_focus(win_id);
                        window_raise(win_id);
                        dirty_all();
                    } else if (window_point_in_body(win_id, mx, my)) {
                        window_set_focus(win_id);
                        window_raise(win_id);
                        window_handle_mouse_click(win_id, mx, my);
                        dirty_window(win_id);
                    }
                }
                dirty_cursor(mx, my);
            }

            if (!now_left)
                drag_window_id = -1;

            if (drag_window_id >= 0) {
                int nx = mx - drag_off_x;
                int ny = my - drag_off_y;
                int ox, oy;
                window_get_rect(drag_window_id, &ox, &oy, 0, 0);
                if (nx != ox || ny != oy) {
                    dirty_window(drag_window_id);
                    window_set_pos(drag_window_id, nx, ny);
                    dirty_window(drag_window_id);
                    dirty_cursor(mx, my);
                }
            }

            if (!now_left && last_left) {
                for (int i = 0; i < MAX_WINDOWS; i++) {
                    if (window_handle_mouse_release(i, mx, my))
                        dirty_window(i);
                }
            }

            last_left = now_left;
        }

        if (dirty_w > 0 && dirty_h > 0)
            apply_dirty(mx, my);

        thread_yield();
    }

    screen_set_terminal_mode(0);
    gfx_backbuffer_free();
}
