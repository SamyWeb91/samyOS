// composer.cpp
#include "composer.h"
#include "gfx.h"
#include "screen.h"
#include "rtc.h"
#include "pit.h"
#include "mouse.h"
#include "win.h"
#include "cpu_monitor.h"
#include "taskbar_start.h"

// =====================================================================
// Colores
// =====================================================================
#define COLOR_DESKTOP_BG     0x0087CEEB
#define COLOR_TASKBAR_BG     0x001A3A5C
#define COLOR_TASKBAR_TEXT   0x00FFFFFF
#define COLOR_TASKBAR_BORDER 0x00000000
#define COLOR_CURSOR_FILL    0x00FFFFFF
#define COLOR_CURSOR_BORDER  0x00000000

#define COLOR_CPU_LOW        0x0034D399
#define COLOR_CPU_MED        0x00FBBF24
#define COLOR_CPU_HIGH       0x00EF4444
#define COLOR_CPU_TRACK      0x002A4A6E

#define TASKBAR_HEIGHT  28
#define CLOCK_RIGHT     10
#define CURSOR_W        8
#define CURSOR_H        12

#define CPU_WIDGET_W    72
#define CPU_TEXT_W      40
#define CPU_BAR_W       20
#define CPU_BAR_H       10

#define TITLEBAR_H      20

/* Zona en la que el menu de inicio puede dibujarse (por encima de la
   taskbar). Se usa para marcar dirty cuando cambia el hover. */
#define START_MENU_ZONE_H 200

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
// Estado del compositor
// =====================================================================
static int term_wid = -1;
static int cpu_percent = 0;

/* --- Dirty region --- */
static int dirty_x = 0, dirty_y = 0, dirty_w = 0, dirty_h = 0;

/* --- Raton --- */
static int mouse_x = 0, mouse_y = 0;
static int last_left = 0;

/* --- Drag de ventanas --- */
enum {
    MOUSE_IDLE = 0,
    MOUSE_DRAG_WIN,
};
static int mouse_mode   = MOUSE_IDLE;
static int drag_wid     = -1;
static int drag_off_x   = 0;
static int drag_off_y   = 0;

// =====================================================================
// Dirty region
// =====================================================================
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

/* Marca la zona donde puede aparecer el menu de inicio. */
static void dirty_start_zone(void) {
    int W = gfx_width(), H = gfx_height();
    int top = H - TASKBAR_HEIGHT - START_MENU_ZONE_H;
    if (top < 0) top = 0;
    dirty_union(0, top, W, H - top);
}

// =====================================================================
// Dibujo del taskbar
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

static void cpu_widget_rect(int *x, int *y, int *w, int *h) {
    int cx, cy, cw, ch;
    clock_rect(&cx, &cy, &cw, &ch);
    *w = CPU_WIDGET_W;
    *h = TASKBAR_HEIGHT - 1;
    *x = cx - 6 - *w;
    *y = cy;
}

static unsigned int cpu_color_for(int pct) {
    if (pct < 30) return COLOR_CPU_LOW;
    if (pct < 70) return COLOR_CPU_MED;
    return COLOR_CPU_HIGH;
}

static void draw_cpu_widget(void) {
    int x, y, w, h;
    cpu_widget_rect(&x, &y, &w, &h);
    gfx_fill_rect(x, y, w, h, COLOR_TASKBAR_BG);

    char buf[9];
    buf[0] = 'C'; buf[1] = 'P'; buf[2] = 'U'; buf[3] = ' ';
    int pct = cpu_percent;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    buf[4] = (pct >= 100) ? '1' : ' ';
    buf[5] = (pct >= 100) ? '0' : ('0' + ((pct / 10) % 10));
    buf[6] = '0' + (pct % 10);
    buf[7] = '%';
    buf[8] = 0;

    int ty = y + (h - 16) / 2;
    gfx_draw_string(x, ty, buf, COLOR_TASKBAR_TEXT, COLOR_TASKBAR_BG);

    int bx = x + CPU_TEXT_W + 2;
    int by = y + (h - CPU_BAR_H) / 2;
    gfx_fill_rect(bx, by, CPU_BAR_W, CPU_BAR_H, COLOR_CPU_TRACK);

    int fill_w = (pct * CPU_BAR_W) / 100;
    if (fill_w > 0)
        gfx_fill_rect(bx, by, fill_w, CPU_BAR_H, cpu_color_for(pct));

    gfx_draw_rect(bx, by, CPU_BAR_W, CPU_BAR_H, COLOR_TASKBAR_TEXT);
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

    int desk_h = H - TASKBAR_HEIGHT;
    if (ry < desk_h) {
        int y0 = ry, h0 = rh;
        if (y0 + h0 > desk_h) h0 = desk_h - y0;
        if (h0 > 0) gfx_fill_rect(rx, y0, rw, h0, COLOR_DESKTOP_BG);
    }

    if (ry + rh > H - TASKBAR_HEIGHT) {
        draw_taskbar();
        taskbar_start_draw();   /* boton Inicio + menu (si abierto) */
        draw_clock();
        draw_cpu_widget();
    }

    win_redraw_all();
}

static void apply_dirty(void) {
    if (dirty_w <= 0 || dirty_h <= 0) return;
    int W = gfx_width(), H = gfx_height();
    int rx = dirty_x, ry = dirty_y, rw = dirty_w, rh = dirty_h;
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > W) rw = W - rx;
    if (ry + rh > H) rh = H - ry;
    if (rw <= 0 || rh <= 0) { dirty_reset(); return; }

    redraw_region(rx, ry, rw, rh);

    int overlap_cursor = !(rx + rw <= mouse_x || rx >= mouse_x + CURSOR_W ||
                           ry + rh <= mouse_y || ry >= mouse_y + CURSOR_H);
    if (overlap_cursor) cursor_draw(mouse_x, mouse_y);

    if (gfx_backbuffer_active())
        gfx_backbuffer_flip_region(rx, ry, rw, rh);

    dirty_reset();
}

// =====================================================================
// API publica
// =====================================================================
extern "C" void composer_init(void) {
    mouse_x = mouse_get_x();
    mouse_y = mouse_get_y();
    last_left = 0;
    mouse_mode = MOUSE_IDLE;
    drag_wid = -1;
    cpu_percent = 0;
    cpu_monitor_init();
    dirty_all();
    apply_dirty();
}

extern "C" void composer_set_terminal(int wid) {
    term_wid = wid;
}

extern "C" int composer_handle_mouse(void) {
    int changed = 0;

    if (!mouse_is_ready()) return 0;

    int nmx = mouse_get_x();
    int nmy = mouse_get_y();

    /* --- Movimiento del raton --- */
    if (nmx != mouse_x || nmy != mouse_y) {
        dirty_union(mouse_x, mouse_y, CURSOR_W, CURSOR_H);
        mouse_x = nmx;
        mouse_y = nmy;
        dirty_union(mouse_x, mouse_y, CURSOR_W, CURSOR_H);
        changed = 1;

        /* Hover del menu de inicio (solo si el raton esta por la zona) */
        if (mouse_y >= gfx_height() - TASKBAR_HEIGHT - START_MENU_ZONE_H) {
            if (taskbar_start_hover(mouse_x, mouse_y)) {
                dirty_start_zone();
                changed = 1;
            }
        }
    }

    int now_left = mouse_left_pressed();
    int click = (now_left && !last_left);

    /* --- Click: primero la UI del taskbar (boton Inicio + menu) --- */
    if (click) {
        if (taskbar_start_click(mouse_x, mouse_y)) {
            dirty_start_zone();
            last_left = now_left;
            return 1;
        }
    }

    /* --- Click: detectar si va a la barra de titulo de una ventana --- */
    if (click) {
        if (mouse_y < gfx_height() - TASKBAR_HEIGHT) {
            int top_wid = -1;
            int top_x = 0, top_y = 0;

            for (int wid = 64; wid >= 1; wid--) {
                int x, y, w, h;
                if (win_get_rect(wid, &x, &y, &w, &h) != 0) continue;
                if (mouse_x >= x && mouse_x < x + w &&
                    mouse_y >= y && mouse_y < y + TITLEBAR_H) {
                    top_wid = wid;
                    top_x = x; top_y = y;
                    break;
                }
            }

            if (top_wid >= 0) {
                mouse_mode = MOUSE_DRAG_WIN;
                drag_wid = top_wid;
                drag_off_x = mouse_x - top_x;
                drag_off_y = mouse_y - top_y;
                win_focus(top_wid);
                dirty_all();
                changed = 1;
            }
        }
    }

    /* --- Drag activo --- */
    if (mouse_mode == MOUSE_DRAG_WIN && drag_wid >= 0) {
        if (now_left) {
            int x, y, w, h;
            if (win_get_rect(drag_wid, &x, &y, &w, &h) == 0) {
                int nx = mouse_x - drag_off_x;
                int ny = mouse_y - drag_off_y;
                if (nx != x || ny != y) {
                    dirty_union(x - 4, y - 4, w + 12, h + 12);
                    win_move(drag_wid, nx, ny);
                    dirty_union(nx - 4, ny - 4, w + 12, h + 12);
                    changed = 1;
                }
            }
        } else {
            mouse_mode = MOUSE_IDLE;
            drag_wid = -1;
            dirty_all();
            changed = 1;
        }
    }

    /* --- Click normal (no en barra de titulo) --- */
    if (click && mouse_mode == MOUSE_IDLE &&
        mouse_y < gfx_height() - TASKBAR_HEIGHT) {
        int fid = win_get_focused();
        int fx = 0, fy = 0, fw = 0, fh = 0;
        if (fid >= 0) win_get_rect(fid, &fx, &fy, &fw, &fh);

        win_handle_click(mouse_x, mouse_y, 1);

        if (fid >= 0 && win_get_focused() != fid) {
            dirty_union(fx - 4, fy - 4, fw + 12, fh + 12);
        } else {
            dirty_all();
        }
        changed = 1;
    }

    last_left = now_left;
    return changed;
}

extern "C" void composer_handle_key(char c) {
    (void)c;
    /* El teclado lo maneja desktop.cpp (term_handle_key). */
}

extern "C" int composer_periodic(unsigned int now_ticks) {
    int changed = 0;

    {
        static unsigned int last_tick = 0;
        if (now_ticks - last_tick >= 100) {
            last_tick = now_ticks;
            int cx, cy, cw, ch;
            clock_rect(&cx, &cy, &cw, &ch);
            dirty_union(cx, cy, cw, ch);
            changed = 1;
        }
    }

    {
        static unsigned int last_cpu = 0;
        if (now_ticks - last_cpu >= 100) {
            last_cpu = now_ticks;
            int cx, cy, cw, ch;
            cpu_widget_rect(&cx, &cy, &cw, &ch);
            dirty_union(cx, cy, cw, ch);
            changed = 1;
        }
    }

    cpu_monitor_begin_iter();
    cpu_monitor_end_iter();
    cpu_percent = cpu_monitor_usage();

    return changed;
}

extern "C" int composer_has_dirty(void) {
    return (dirty_w > 0 && dirty_h > 0);
}

extern "C" void composer_apply(void) {
    apply_dirty();
}

extern "C" void composer_dirty_window(int wid) {
    int x, y, w, h;
    if (win_get_rect(wid, &x, &y, &w, &h) == 0) {
        dirty_union(x - 4, y - 4, w + 12, h + 12);
    }
}

extern "C" void composer_force_redraw(void) {
    dirty_all();
    apply_dirty();
}
