#include "screen.h"
#include "gfx.h"
#include "vbe.h"
#include <stdarg.h>
#include <stdint.h>

// =====================================================================
// Buffer de texto VGA (0xB8000) — usado cuando NO hay gfx
// =====================================================================
#define VGA_MEM    ((volatile uint16_t *)0xB8000)
#define VGA_W      80
#define VGA_H      25

static int cursor_x = 0;
static int cursor_y = 0;

static uint8_t text_color = 0x07;   // gris claro sobre negro
static uint8_t back_color = 0x00;

// =====================================================================
// Estado de gfx (modo gráfico)
// =====================================================================
static int gfx_mode = 0;   // 1 si estamos en modo gráfico

// Cursor en modo gráfico (en píxeles)
static int gfx_cursor_x = 0;
static int gfx_cursor_y = 0;

#define GFX_CHAR_W  8
#define GFX_CHAR_H  16
#define GFX_MARGIN  4

// Colores del framebuffer (RRGGBB)
#define GFX_FG_COLOR  0x00CCCCCC   // gris claro
#define GFX_BG_COLOR  0x00000000   // negro

// =====================================================================
// Internos
// =====================================================================
static void screen_update_cursor_vga(void) {
    uint16_t pos = cursor_y * VGA_W + cursor_x;
    // Puerto del cursor VGA (0x3D4 / 0x3D5)
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x0F), "Nd"((uint16_t)0x3D4));
    asm volatile("outb %0, %1" : : "a"((uint8_t)(pos & 0xFF)), "Nd"((uint16_t)0x3D5));
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x0E), "Nd"((uint16_t)0x3D4));
    asm volatile("outb %0, %1" : : "a"((uint8_t)((pos >> 8) & 0xFF)), "Nd"((uint16_t)0x3D5));
}

static void screen_scroll_vga(void) {
    if (cursor_y < VGA_H) return;

    // Mover todo el buffer una línea hacia arriba
    for (int i = 0; i < (VGA_H - 1) * VGA_W; i++) {
        VGA_MEM[i] = VGA_MEM[i + VGA_W];
    }
    // Limpiar la última línea
    for (int i = (VGA_H - 1) * VGA_W; i < VGA_H * VGA_W; i++) {
        VGA_MEM[i] = (back_color << 12) | (text_color << 8) | ' ';
    }
    cursor_y = VGA_H - 1;
}

static void screen_scroll_gfx(void) {
    // Scroll en modo gráfico: desplazar el framebuffer hacia arriba
    // GFX_CHAR_H píxeles.
    if (gfx_cursor_y + GFX_CHAR_H <= gfx_height()) return;

    // Re-dibujar toda la pantalla desde la línea siguiente.
    // (simple, pero funciona)
    // Alternativa más eficiente: memmove en el framebuffer.
    // Por ahora, solo lo dejamos "limpio" con scroll no-op.

    // Scroll simple: limpiar y empezar arriba
    gfx_clear(GFX_BG_COLOR);
    gfx_cursor_x = GFX_MARGIN;
    gfx_cursor_y = GFX_MARGIN;
}

// =====================================================================
// Init
// =====================================================================
// Llamar a esto después de gfx_init() si VBE está disponible.
extern "C" void screen_set_gfx_mode(void) {
    if (vbe_available() && gfx_ready()) {
        gfx_mode = 1;
        gfx_cursor_x = GFX_MARGIN;
        gfx_cursor_y = GFX_MARGIN;
    }
}

// =====================================================================
// API pública
// =====================================================================
void screen_clear(void) {
    if (gfx_mode) {
        gfx_clear(GFX_BG_COLOR);
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
                gfx_draw_char(gfx_cursor_x, gfx_cursor_y, ' ',
                              GFX_FG_COLOR, GFX_BG_COLOR);
            }
            return;
        }

        gfx_draw_char(gfx_cursor_x, gfx_cursor_y, c,
                      GFX_FG_COLOR, GFX_BG_COLOR);

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
    while (*str) {
        screen_putchar(*str);
        str++;
    }
}

// =====================================================================
// kprintf (formato mínimo: %s, %c, %u, %d, %x, %p, %%)
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
