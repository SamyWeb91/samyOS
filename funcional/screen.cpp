#include "screen.h"

static volatile char *video = (volatile char*)0xB8000;
static int cursor = 0;

void screen_clear() {
    for (int i = 0; i < 80 * 25; i++) {
        video[i * 2] = ' ';
        video[i * 2 + 1] = 0x07;
    }
    cursor = 0;
}

static void scroll() {
    for (int i = 0; i < 80 * 24; i++) {
        video[i * 2] = video[(i + 80) * 2];
        video[i * 2 + 1] = video[(i + 80) * 2 + 1];
    }
    for (int i = 80 * 24; i < 80 * 25; i++) {
        video[i * 2] = ' ';
        video[i * 2 + 1] = 0x07;
    }
    cursor = 80 * 24;
}

void screen_putchar(char c) {
    if (c == '\n') {
        cursor += 80 - (cursor % 80);
        if (cursor >= 80 * 25) scroll();
        return;
    }
    if (c == '\r') {
        cursor -= cursor % 80;
        return;
    }
    if (c == '\b') {
        if (cursor > 0) {
            cursor--;
            video[cursor * 2] = ' ';
            video[cursor * 2 + 1] = 0x07;
        }
        return;
    }
    video[cursor * 2] = c;
    video[cursor * 2 + 1] = 0x07;
    cursor++;
    if (cursor >= 80 * 25) scroll();
}

void screen_write(const char *str) {
    while (*str) screen_putchar(*str++);
}

// =====================================================================
// Helpers de kprintf
// =====================================================================
static void print_pad_hex(unsigned int v, int width) {
    const char *hex = "0123456789ABCDEF";
    char buf[16];
    int i = 0;
    if (v == 0) { buf[i++] = '0'; }
    else {
        while (v > 0) { buf[i++] = hex[v & 0xF]; v >>= 4; }
    }
    // Rellenar con ceros a la izquierda hasta width
    while (i < width) buf[i++] = '0';
    // Imprimir en orden inverso
    while (i > 0) screen_putchar(buf[--i]);
}

static void print_pad_dec(unsigned int v, int width, char pad) {
    char buf[16];
    int i = 0;
    if (v == 0) { buf[i++] = '0'; }
    else {
        while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
    }
    while (i < width) buf[i++] = pad;
    while (i > 0) screen_putchar(buf[--i]);
}

static void print_hex(unsigned int v) {
    screen_write("0x");
    print_pad_hex(v, 0);
}

static void print_dec(unsigned int v) {
    print_pad_dec(v, 0, ' ');
}

static void print_sdec(int v) {
    if (v < 0) { screen_putchar('-'); print_dec((unsigned int)(-v)); }
    else print_dec((unsigned int)v);
}

// =====================================================================
// kprintf con soporte de ancho: %u, %d, %x, %s, %c, %02u, %08x, etc.
// =====================================================================
void kprintf(const char *fmt, ...) {
    unsigned int *args = (unsigned int*)(&fmt) + 1;
    int argi = 0;

    while (*fmt) {
        if (*fmt != '%') {
            screen_putchar(*fmt++);
            continue;
        }
        fmt++;

        // --- Detectar relleno con ceros ---
        char pad = ' ';
        if (*fmt == '0') { pad = '0'; fmt++; }

        // --- Detectar ancho ---
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        // --- Especificador ---
        switch (*fmt) {
            case 's': screen_write((const char*)args[argi++]); break;
            case 'd': print_sdec((int)args[argi++]); break;
            case 'u':
                if (width > 0) print_pad_dec(args[argi++], width, pad);
                else print_dec(args[argi++]);
                break;
            case 'x':
                if (width > 0) print_pad_hex(args[argi++], width);
                else print_hex(args[argi++]);
                break;
            case 'c': screen_putchar((char)args[argi++]); break;
            case '%': screen_putchar('%'); break;
            default:  screen_putchar('%'); screen_putchar(*fmt); break;
        }
        fmt++;
    }
}
