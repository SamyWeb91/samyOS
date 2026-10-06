#include "screen.h"
#include "gfx.h"
#include "vbe.h"
#include <stdarg.h>
#include <stdint.h>

// =====================================================================
// Buffer VGA (modo texto) — solo si no hay VBE
// =====================================================================
#define VGA_MEM    ((volatile uint16_t *)0xB8000)
#define VGA_W      80
#define VGA_H      25

static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t text_color = 0x07;
static uint8_t back_color = 0x00;

// =====================================================================
// Estado del modo gráfico
// =====================================================================
static int gfx_mode = 0;

static int gfx_cursor_x = 0;
static int gfx_cursor_y = 0;

static uint32_t gfx_fg = 0x00CCCCCC;
static uint32_t gfx_bg = 0x00000000;

#define GFX_CHAR_W  8
#define GFX_CHAR_H  16
#define GFX_MARGIN  4

// =====================================================================
// Helpers VGA
// =====================================================================
static void screen_update_cursor_vga(void) {
    uint16_t pos = cursor_y * VGA_W + cursor_x;
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x0F), "Nd"((uint16_t)0x3D4));
    asm volatile("outb %0, %1" : : "a"((uint8_t)(pos & 0xFF)), "Nd"((uint16_t)0x3D5));
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x0E), "Nd"((uint16_t)0x3D4));
    asm volatile("outb %0, %1" : : "a"((uint8_t)((pos >> 8) & 0xFF)), "Nd"((uint16_t)0x3D5));
}

static void screen_scroll_vga(void) {
    if (cursor_y < VGA_H) return;

    for (int i = 0; i < (VGA_H - 1) * VGA_W; i++) {
        VGA_MEM[i] = VGA_MEM[i + VGA_W];
    }
    for (int i = (VGA_H - 1) * VGA_W; i < VGA_H * VGA_W; i++) {
        VGA_MEM[i] = (back_color << 12) | (text_color << 8) | ' ';
    }
    cursor_y = VGA_H - 1;
}

// =====================================================================
// Scroll gráfico
// =====================================================================
static void screen_scroll_gfx(void) {
    if (gfx_cursor_y + GFX_CHAR_H <= gfx_height()) return;

    int W = gfx_width();
    int H = gfx_height();

    gfx_copy_rect(0, GFX_CHAR_H, 0, 0, W, H - GFX_CHAR_H);
    gfx_fill_rect(0, H - GFX_CHAR_H, W, GFX_CHAR_H, gfx_bg);

    gfx_cursor_y -= GFX_CHAR_H;
    if (gfx_cursor_y < GFX_MARGIN) gfx_cursor_y = GFX_MARGIN;
}

// =====================================================================
// Set modo
//
// SIN extern "C" — linkage C++ por defecto (coincide con screen.h).
// =====================================================================
void screen_set_gfx_mode(void) {
    if (vbe_available() && gfx_ready()) {
        gfx_mode = 1;
        gfx_cursor_x = GFX_MARGIN;
        gfx_cursor_y = GFX_MARGIN;
        gfx_clear(gfx_bg);
    }
}

void screen_set_colors(unsigned int fg, unsigned int bg) {
    gfx_fg = fg & 0x00FFFFFF;
    gfx_bg = bg & 0x00FFFFFF;
}

// =====================================================================
// API pública
// =====================================================================
void screen_clear(void) {
    if (gfx_mode) {
        gfx_clear(gfx_bg);
        gfx_cursor_x = GFX_MARGIN;
        gfx_cursor_y = GFX_MARGIN;
        return;
    }

    for (int i = 0; i < VGA_W * VGA_H; i++) {
        VGA_MEM[i] = (back_color << 12) | (text_color << 8) | ' ';
    }
    cursor_x = 0;
    cursor_y = 0;
    screen_update_cursor_vga();
}

void screen_putchar(char c) {
    if (gfx_mode) {
        if (c == '\n') {
            gfx_cursor_x = GFX_MARGIN;
            gfx_cursor_y += GFX_CHAR_H;
            if (gfx_cursor_y + GFX_CHAR_H > gfx_height()) {
                screen_scroll_gfx();
            }
            return;
        }
        if (c == '\r') {
            gfx_cursor_x = GFX_MARGIN;
            return;
        }
        if (c == '\b') {
            if (gfx_cursor_x > GFX_MARGIN) {
                gfx_cursor_x -= GFX_CHAR_W;
                gfx_fill_rect(gfx_cursor_x, gfx_cursor_y,
                              GFX_CHAR_W, GFX_CHAR_H, gfx_bg);
            }
            return;
        }

        gfx_draw_char(gfx_cursor_x, gfx_cursor_y, c, gfx_fg, gfx_bg);

        gfx_cursor_x += GFX_CHAR_W;
        if (gfx_cursor_x + GFX_CHAR_W > gfx_width()) {
            gfx_cursor_x = GFX_MARGIN;
            gfx_cursor_y += GFX_CHAR_H;
            if (gfx_cursor_y + GFX_CHAR_H > gfx_height()) {
                screen_scroll_gfx();
            }
        }
        return;
    }

    // Modo texto VGA
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            VGA_MEM[cursor_y * VGA_W + cursor_x] =
                (back_color << 12) | (text_color << 8) | ' ';
        }
    } else {
        VGA_MEM[cursor_y * VGA_W + cursor_x] =
            (back_color << 12) | (text_color << 8) | (uint8_t)c;
        cursor_x++;
        if (cursor_x >= VGA_W) {
            cursor_x = 0;
            cursor_y++;
        }
    }

    screen_scroll_vga();
    screen_update_cursor_vga();
}

void screen_write(const char *str) {
    while (*str) screen_putchar(*str++);
}

// =====================================================================
// kprintf
// =====================================================================
static void kprint_u32(unsigned int v, int base, int pad, char padc) {
    char buf[32];
    int i = 0;

    if (v == 0) {
        buf[i++] = '0';
    } else {
        const char *digits = "0123456789abcdef";
        while (v > 0) {
            buf[i++] = digits[v % base];
            v /= base;
        }
    }
    while (i < pad) buf[i++] = padc;
    while (i > 0) {
        i--;
        screen_putchar(buf[i]);
    }
}

static void kprint_i32(int v) {
    if (v < 0) {
        screen_putchar('-');
        kprint_u32((unsigned int)(-v), 10, 0, ' ');
    } else {
        kprint_u32((unsigned int)v, 10, 0, ' ');
    }
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            screen_putchar(*fmt++);
            continue;
        }
        fmt++;

        int pad = 0;
        char padc = ' ';

        if (*fmt == '0') { padc = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') {
            pad = pad * 10 + (*fmt - '0');
            fmt++;
        }

        switch (*fmt) {
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s) screen_putchar(*s++);
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                screen_putchar(c);
                break;
            }
            case 'u': {
                unsigned int v = va_arg(ap, unsigned int);
                kprint_u32(v, 10, pad, padc);
                break;
            }
            case 'd': {
                int v = va_arg(ap, int);
                kprint_i32(v);
                break;
            }
            case 'x': {
                unsigned int v = va_arg(ap, unsigned int);
                kprint_u32(v, 16, pad, padc);
                break;
            }
            case 'p': {
                unsigned int v = (unsigned int)va_arg(ap, void *);
                kprint_u32(v, 16, 8, '0');
                break;
            }
            case '%': {
                screen_putchar('%');
                break;
            }
            default:
                screen_putchar('%');
                screen_putchar(*fmt);
                break;
        }
        fmt++;
    }

    va_end(ap);
}
