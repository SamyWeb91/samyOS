// Copyright (C) 2026 Samuel Tamayo Infante
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "screen.h"
#include "gfx.h"
#include "vbe.h"
#include "pit.h"
#include "terminal.h"
#include <stdarg.h>
#include <stdint.h>

// =====================================================================
// Callbacks implementados en desktop.cpp
// Redirigen la salida al subsistema grafico actual
// =====================================================================
extern "C" void desktop_terminal_putchar(char c);
extern "C" void desktop_terminal_clear(void);

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
// Redireccion a la terminal
// =====================================================================
static int use_terminal = 0;

void screen_set_terminal_mode(int on) { use_terminal = on; }
int  screen_get_terminal_mode(void)   { return use_terminal; }

// =====================================================================
// Optimizacion del arranque:
// no hacer flip por cada caracter (3 MB de copia cada vez).
// Hacerlo cada N caracteres, y un flip completo al final.
// =====================================================================
#define FLIP_EVERY_N_CHARS  64
static int chars_since_flip = 0;

// =====================================================================
// Cursor parpadeante (modo grafico a pantalla completa)
// =====================================================================
#define CURSOR_BLINK_TICKS  50

static int          cursor_visible   = 1;
static unsigned int cursor_last_tick = 0;

static void draw_cursor_gfx(uint32_t color) {
    if (!gfx_mode) return;
    if (use_terminal) return;
    if (gfx_cursor_x <= 0) return;
    gfx_fill_rect(gfx_cursor_x, gfx_cursor_y + GFX_CHAR_H - 2,
                  GFX_CHAR_W, 2, color);
}

static void erase_cursor_gfx(void) {
    draw_cursor_gfx(gfx_bg);
}

static void update_cursor_blink(void) {
    if (!gfx_mode) return;
    if (use_terminal) return;

    unsigned int now = pit_get_ticks();
    if (now - cursor_last_tick >= CURSOR_BLINK_TICKS) {
        cursor_last_tick = now;
        cursor_visible = !cursor_visible;

        if (cursor_visible) draw_cursor_gfx(gfx_fg);
        else                erase_cursor_gfx();
    }
}

static void move_cursor_gfx(void) {
    if (!gfx_mode) return;
    if (use_terminal) return;
    cursor_visible = 1;
    cursor_last_tick = pit_get_ticks();
    draw_cursor_gfx(gfx_fg);
}

void screen_blink_tick(void) {
    update_cursor_blink();
}

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
// API que screen.cpp espera (redirige al terminal gráfico)
// =====================================================================
extern "C" void desktop_terminal_clear(void) {
    if (term_wid >= 0) {
        win_sys_clear(term_wid);
        term_prompt();
        int x, y, w, h;
        if (win_get_rect(term_wid, &x, &y, &w, &h) == 0)
            dirty_union(x, y, w, h);
    }
}

extern "C" void desktop_terminal_putchar(char c) {
    if (term_wid < 0) return;

    if (c == '\n') {
        win_sys_write(term_wid, "\n", 1);
    } else if (c == '\b') {
        win_sys_write(term_wid, "\b", 1);
    } else if (c >= 32 && c < 127) {
        char ch[2] = { c, 0 };
        win_sys_write(term_wid, ch, 1);
    }

    int x, y, w, h;
    if (win_get_rect(term_wid, &x, &y, &w, &h) == 0)
        dirty_union(x, y, w, h);
}
// =====================================================================
// Scroll grafico
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
// =====================================================================
void screen_set_gfx_mode(void) {
    if (vbe_available() && gfx_ready()) {
        gfx_mode = 1;
        gfx_cursor_x = GFX_MARGIN;
        gfx_cursor_y = GFX_MARGIN;
        cursor_visible = 1;
        cursor_last_tick = pit_get_ticks();
        gfx_clear(gfx_bg);
        draw_cursor_gfx(gfx_fg);
    }
}

void screen_set_colors(unsigned int fg, unsigned int bg) {
    gfx_fg = fg & 0x00FFFFFF;
    gfx_bg = bg & 0x00FFFFFF;
}

// =====================================================================
// API publica
// =====================================================================
void screen_clear(void) {
    if (use_terminal) {
        desktop_terminal_clear();   // ← antes: terminal_clear();
        return;
    }

    if (gfx_mode) {
        gfx_clear(gfx_bg);
        gfx_cursor_x = GFX_MARGIN;
        gfx_cursor_y = GFX_MARGIN;
        cursor_visible = 1;
        cursor_last_tick = pit_get_ticks();
        draw_cursor_gfx(gfx_fg);
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
    if (use_terminal) {
        desktop_terminal_putchar(c);  // ← antes: terminal_putchar(c);
        return;
    }

    update_cursor_blink();

    if (gfx_mode) {
        erase_cursor_gfx();

        if (c == '\n') {
            gfx_cursor_x = GFX_MARGIN;
            gfx_cursor_y += GFX_CHAR_H;
            if (gfx_cursor_y + GFX_CHAR_H > gfx_height()) {
                screen_scroll_gfx();
            }
            move_cursor_gfx();
            goto flip;
        }
        if (c == '\r') {
            gfx_cursor_x = GFX_MARGIN;
            move_cursor_gfx();
            goto flip;
        }
        if (c == '\b') {
            if (gfx_cursor_x > GFX_MARGIN) {
                gfx_cursor_x -= GFX_CHAR_W;
                gfx_fill_rect(gfx_cursor_x, gfx_cursor_y,
                              GFX_CHAR_W, GFX_CHAR_H, gfx_bg);
            }
            move_cursor_gfx();
            goto flip;
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
        move_cursor_gfx();

    flip:
        chars_since_flip++;
        if (chars_since_flip >= FLIP_EVERY_N_CHARS) {
            chars_since_flip = 0;
            if (gfx_backbuffer_active()) {
                gfx_backbuffer_flip();
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
