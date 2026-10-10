#include "desktop.h"
#include "gfx.h"
#include "screen.h"
#include "rtc.h"
#include "pit.h"
#include "keyboard.h"
#include "thread.h"
#include "mouse.h"
#include "win.h"
#include "cpu_monitor.h"

// =====================================================================
// Colores
// =====================================================================
#define COLOR_DESKTOP_BG     0x0087CEEB
#define COLOR_TASKBAR_BG     0x001A3A5C
#define COLOR_TASKBAR_TEXT   0x00FFFFFF
#define COLOR_TASKBAR_BORDER 0x00000000
#define COLOR_CURSOR_FILL    0x00FFFFFF
#define COLOR_CURSOR_BORDER  0x00000000

#define COLOR_CPU_LOW        0x0034D399   /* verde  */
#define COLOR_CPU_MED        0x00FBBF24   /* ámbar  */
#define COLOR_CPU_HIGH       0x00EF4444   /* rojo   */
#define COLOR_CPU_TRACK      0x002A4A6E   /* fondo barra */

#define TASKBAR_HEIGHT  28
#define CLOCK_RIGHT     10
#define CURSOR_W        8
#define CURSOR_H        12

/* Ancho total del widget de CPU (texto + barra) en píxeles */
#define CPU_WIDGET_W    72
#define CPU_TEXT_W      40
#define CPU_BAR_W       20
#define CPU_BAR_H       10

// =====================================================================
// Helpers de string
// =====================================================================
static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static int str_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++; prefix++;
    }
    return 1;
}

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
// Variable global de la ventana terminal
// =====================================================================
static int term_wid = -1;

extern "C" void desktop_terminal_putchar(char c) {
    if (term_wid < 0) return;
    win_sys_write(term_wid, &c, 1);
}

extern "C" void desktop_terminal_clear(void) {
    if (term_wid < 0) return;
    win_sys_clear(term_wid);
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

// =====================================================================
// Estadísticas de CPU (delegadas a cpu_monitor.cpp)
// =====================================================================
static int cpu_percent = 0;

static void cpu_stats_reset(void) {
    cpu_monitor_init();
    cpu_percent = 0;
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

/* ---------- Widget de CPU ---------- */
static void cpu_widget_rect(int *x, int *y, int *w, int *h) {
    int W = gfx_width(), H = gfx_height();
    int cx, cy, cw, ch;
    clock_rect(&cx, &cy, &cw, &ch);
    *w = CPU_WIDGET_W;
    *h = TASKBAR_HEIGHT - 1;
    *x = cx - 6 - *w;
    *y = cy;
    (void)W; (void)H;
}

static unsigned int cpu_color_for(int pct) {
    if (pct < 30) return COLOR_CPU_LOW;
    if (pct < 70) return COLOR_CPU_MED;
    return COLOR_CPU_HIGH;
}

static void draw_cpu_widget(void) {
    int x, y, w, h;
    cpu_widget_rect(&x, &y, &w, &h);

    /* Fondo */
    gfx_fill_rect(x, y, w, h, COLOR_TASKBAR_BG);

    /* Texto: "CPU xxx%" */
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

    /* Barra horizontal a la derecha del texto */
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
        draw_clock();
        draw_cpu_widget();
    }

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

    if (gfx_backbuffer_active())
        gfx_backbuffer_flip_region(rx, ry, rw, rh);

    dirty_reset();
}

// =====================================================================
// Shell gráfica (dentro de una ventana)
// =====================================================================
#define TERM_LINE_MAX  64
static char  term_line[TERM_LINE_MAX];
static int   term_len = 0;

static void term_print(const char *s) {
    if (term_wid < 0) return;
    int n = 0;
    while (s[n]) n++;
    win_sys_write(term_wid, s, n);
}

static void term_println(const char *s) {
    term_print(s);
    term_print("\n");
}

static void term_prompt(void) {
    term_print("> ");
}

static void term_execute(const char *cmd) {
    if (cmd[0] == 0) return;

    if (str_eq(cmd, "help")) {
        term_println("Comandos graficos:");
        term_println("  help     - esta ayuda");
        term_println("  clear    - limpiar terminal");
        term_println("  date     - fecha y hora");
        term_println("  ticks    - ticks del sistema");
        term_println("  uptime   - tiempo desde arranque");
        term_println("  echo ... - imprimir texto");
        term_println("  close    - cerrar esta ventana");
        term_println("  reboot   - reiniciar");
    }
    else if (str_eq(cmd, "clear")) {
        win_sys_clear(term_wid);
    }
    else if (str_eq(cmd, "date")) {
        RTCTime t; rtc_read(&t);
        char buf[48];
        buf[0] = '0' + (t.day/10); buf[1] = '0' + (t.day%10); buf[2] = '/';
        buf[3] = '0' + (t.month/10); buf[4] = '0' + (t.month%10); buf[5] = '/';
        buf[6] = '0' + ((t.year/1000)%10); buf[7] = '0' + ((t.year/100)%10);
        buf[8] = '0' + ((t.year/10)%10); buf[9] = '0' + (t.year%10);
        buf[10] = ' '; buf[11] = '0' + (t.hour/10); buf[12] = '0' + (t.hour%10);
        buf[13] = ':'; buf[14] = '0' + (t.minute/10); buf[15] = '0' + (t.minute%10);
        buf[16] = ':'; buf[17] = '0' + (t.second/10); buf[18] = '0' + (t.second%10);
        buf[19] = 0;
        term_println(buf);
    }
    else if (str_eq(cmd, "ticks")) {
        unsigned int t = pit_get_ticks();
        char tmp[12];
        int n = 0;
        if (t == 0) {
            tmp[n++] = '0';
        } else {
            while (t) {
                tmp[n++] = '0' + (t % 10);
                t /= 10;
            }
        }
        char buf[24];
        buf[0] = 'T'; buf[1] = 'i'; buf[2] = 'c'; buf[3] = 'k';
        buf[4] = 's'; buf[5] = ':'; buf[6] = ' ';
        for (int k = 0; k < n; k++) buf[7 + k] = tmp[n - 1 - k];
        buf[7 + n] = 0;
        term_println(buf);
    }
    else if (str_eq(cmd, "uptime")) {
        unsigned int t = pit_get_ticks() / 100;
        unsigned int h = t / 3600;
        unsigned int m = (t % 3600) / 60;
        unsigned int s = t % 60;
        char buf[20];
        buf[0] = '0' + (h / 10); buf[1] = '0' + (h % 10); buf[2] = ':';
        buf[3] = '0' + (m / 10); buf[4] = '0' + (m % 10); buf[5] = ':';
        buf[6] = '0' + (s / 10); buf[7] = '0' + (s % 10); buf[8] = 0;
        term_print("Uptime: ");
        term_println(buf);
    }
    else if (str_starts_with(cmd, "echo ")) {
        term_println(cmd + 5);
    }
    else if (str_eq(cmd, "close")) {
        if (term_wid >= 0) {
            int x, y, w, h;
            win_get_rect(term_wid, &x, &y, &w, &h);
            win_sys_destroy(term_wid);
            term_wid = -1;
            dirty_union(x, y, w + 8, h + 8);
        }
        return;
    }
    else if (str_eq(cmd, "reboot")) {
        term_println("Reiniciando...");
    }
    else {
        term_print("Desconocido: ");
        term_println(cmd);
    }
}

static void term_handle_key(char c) {
    if (term_wid < 0) return;

    if (c == '\n') {
        win_sys_write(term_wid, "\n", 1);
        term_line[term_len] = 0;
        term_execute(term_line);
        term_len = 0;
        if (term_wid >= 0) term_prompt();
        int x, y, w, h;
        if (win_get_rect(term_wid, &x, &y, &w, &h) == 0)
            dirty_union(x, y, w, h);
    }
    else if (c == '\b') {
        if (term_len > 0) {
            term_len--;
            win_sys_write(term_wid, "\b", 1);
            int x, y, w, h;
            if (win_get_rect(term_wid, &x, &y, &w, &h) == 0)
                dirty_union(x, y, w, h);
        }
    }
    else if (c >= 32 && c < 127) {
        if (term_len < TERM_LINE_MAX - 1) {
            term_line[term_len++] = c;
            char ch[2] = { c, 0 };
            win_sys_write(term_wid, ch, 1);
            int x, y, w, h;
            if (win_get_rect(term_wid, &x, &y, &w, &h) == 0)
                dirty_union(x, y, w, h);
        }
    }
}

// =====================================================================
// Dormir la CPU hasta la próxima interrupción
// =====================================================================
static inline void cpu_idle_wait(void) {
    __asm__ __volatile__("sti; hlt");
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
    int last_btn = 0;

    /* ---------- Crear terminal gráfica ---------- */
    term_wid = win_sys_create(80, 60, 420, 320, "Terminal");
    kprintf("desktop: term_wid = %d\n", term_wid);

    if (term_wid >= 0) {
        term_println("MiniSO - Terminal grafica");
        term_println("Escribe 'help' para ver comandos");
        term_println("");
        term_prompt();
    }

    dirty_all();
    apply_dirty(mx, my);

    unsigned int last_tick = pit_get_ticks();
    cpu_stats_reset();

    while (1) {
        int did_work = 0;
        unsigned int now = pit_get_ticks();

        /* ---------- Reloj: refresco 1 vez por segundo ---------- */
        if (now - last_tick >= 100) {
            last_tick = now;
            int cx, cy, cw, ch;
            clock_rect(&cx, &cy, &cw, &ch);
            dirty_union(cx, cy, cw, ch);
            did_work = 1;
        }

        /* ---------- Widget de CPU: refresco 1 vez por segundo ---------- */
        {
            static unsigned int last_cpu_tick = 0;
            if (now - last_cpu_tick >= 100) {
                last_cpu_tick = now;
                int cx, cy, cw, ch;
                cpu_widget_rect(&cx, &cy, &cw, &ch);
                dirty_union(cx, cy, cw, ch);
            }
        }

        /* ---------- Ratón ---------- */
        if (mouse_is_ready()) {
            int nmx = mouse_get_x();
            int nmy = mouse_get_y();

            if (nmx != mx || nmy != my) {
                dirty_union(mx, my, CURSOR_W, CURSOR_H);
                mx = nmx; my = nmy;
                dirty_union(mx, my, CURSOR_W, CURSOR_H);
                did_work = 1;
            }

            int now_left = mouse_left_pressed();
            int click = (now_left && !last_btn);

            if (click) {
                int fx = 0, fy = 0, fw = 0, fh = 0;
                int fid = win_get_focused();
                if (fid >= 0)
                    win_get_rect(fid, &fx, &fy, &fw, &fh);

                win_handle_click(mx, my, 1);

                if (fid >= 0 && win_get_focused() != fid) {
                    dirty_union(fx - 4, fy - 4, fw + 12, fh + 12);
                    if (fid == term_wid) term_wid = -1;
                } else {
                    dirty_all();
                }
                did_work = 1;
            }
            last_btn = now_left;
        }

        /* ---------- Teclado → terminal gráfica ---------- */
        char c = keyboard_getchar();
        if (c) {
            term_handle_key(c);
            did_work = 1;
        }

        /* ---------- Aplicar dirty si hay algo pendiente ---------- */
        if (dirty_w > 0 && dirty_h > 0) {
            apply_dirty(mx, my);
            did_work = 1;
        }

        /* ---------- Contabilidad de CPU ---------- */
        cpu_monitor_begin_iter();
        cpu_monitor_end_iter();
        cpu_percent = cpu_monitor_usage();

        /* ---------- Dormir o ceder la CPU ---------- */
        if (!did_work) {
            cpu_monitor_sleep_start();
            cpu_idle_wait();
            cpu_monitor_sleep_end();
        } else {
            thread_yield();
        }
    }
}
