#include "gfx_console.h"
#include "gfx.h"

#define COLS  128     // 1024 / 8
#define ROWS   96     // 768 / 8

#define FG_COLOR  0x00C0C0C0
#define BG_COLOR  0x00102040

static int cursor_x = 0;
static int cursor_y = 0;

static void scroll() {
    // Copiar todas las filas una hacia arriba (8 pixeles)
    for (int y = 0; y < (ROWS - 1) * 8; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            gfx_putpixel(x, y, gfx_getpixel(x, y + 8));
        }
    }
    // Limpiar la ultima fila
    for (int y = (ROWS - 1) * 8; y < ROWS * 8; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            gfx_putpixel(x, y, BG_COLOR);
        }
    }
    cursor_y = ROWS - 1;
}

void console_clear() {
    gfx_clear(BG_COLOR);
    cursor_x = 0;
    cursor_y = 0;
}

void console_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= ROWS) scroll();
        return;
    }

    if (c == '\r') {
        cursor_x = 0;
        return;
    }

    gfx_draw_char(cursor_x * 8, cursor_y * 8, c, FG_COLOR, BG_COLOR);

    cursor_x++;
    if (cursor_x >= COLS) {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= ROWS) scroll();
    }
}

void console_write(const char *s) {
    while (*s) console_putchar(*s++);
}

void console_printf(const char *fmt, ...) {
    unsigned int *args = (unsigned int*)(&fmt) + 1;
    int argi = 0;
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's': console_write((const char*)args[argi++]); break;
                case 'd': {
                    int v = (int)args[argi++];
                    if (v < 0) { console_putchar('-'); v = -v; }
                    char buf[12]; int i = 0;
                    if (v == 0) buf[i++] = '0';
                    while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
                    while (i > 0) console_putchar(buf[--i]);
                    break;
                }
                case 'x': {
                    unsigned int v = args[argi++];
                    const char *hex = "0123456789ABCDEF";
                    console_write("0x");
                    for (int i = 7; i >= 0; i--)
                        console_putchar(hex[(v >> (i * 4)) & 0xF]);
                    break;
                }
                case 'c': console_putchar((char)args[argi++]); break;
                case '%': console_putchar('%'); break;
                default: console_putchar('%'); console_putchar(*fmt); break;
            }
        } else {
            console_putchar(*fmt);
        }
        fmt++;
    }
}
