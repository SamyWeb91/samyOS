#include "desktop.h"
#include "gfx.h"
#include "screen.h"
#include "rtc.h"
#include "pit.h"
#include "keyboard.h"
#include "thread.h"
#include "mouse.h"

// =====================================================================
// Colores del escritorio
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

// Bitmap del cursor: 0=transparente, 1=blanco, 2=negro
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

    gfx_fill_rect(0, 0, W, H - TASKBAR_HEIGHT, COLOR_DESKTOP_BG);
    draw_taskbar();
    draw_clock();

    gfx_draw_string(20, 20, "MiniOS Desktop", 0x00FFFFFF, COLOR_DESKTOP_BG);
    gfx_draw_string(20, 40, "Pulsa ESC para volver a la shell.",
                    0x00FFFFFF, COLOR_DESKTOP_BG);

    // Cursor inicial en el centro
    cursor_x = -1;
    cursor_y = -1;
    cursor_move(W / 2, H / 2);

    unsigned int last_tick = pit_get_ticks();

    while (1) {
        char c = keyboard_getchar();
        if (c == 27) break;

        unsigned int now = pit_get_ticks();
        if (now - last_tick >= 100) {
            last_tick = now;
            draw_clock();
        }

        if (mouse_is_ready()) {
            int mx = mouse_get_x();
            int my = mouse_get_y();
            cursor_move(mx, my);
        }

        thread_yield();
    }

    screen_clear();
}
