// test_ahci.c
// Test automatico: init minimo, ahci_init, imprime todo en framebuffer VBE.

#include "gdt.h"
#include "tss.h"
#include "paging.h"
#include "heap.h"
#include "ahci.h"

/* Fuente */
extern "C" uint8_t font8x16[256][16];

/* Stubs por si el linker los pide */
extern "C" void desktop_terminal_putchar(char c) { (void)c; }
extern "C" void desktop_terminal_clear(void) { }
extern "C" void schedule(void) { }
void kprintf(const char *fmt, ...) { (void)fmt; }

/* =====================================================================
 * Puertos I/O
 * ===================================================================== */
static inline void io_outb(unsigned short port, unsigned char v) {
    __asm__ __volatile__("outb %0, %1" : : "a"(v), "Nd"(port));
}
static inline unsigned char io_inb(unsigned short port) {
    unsigned char v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

/* =====================================================================
 * Salida por SERIE (COM1) — para ver algo aunque falle el framebuffer
 * ===================================================================== */
static void serial_init(void) {
    io_outb(0x3F9, 0x00);
    io_outb(0x3FB, 0x80);
    io_outb(0x3F8, 0x03);
    io_outb(0x3F9, 0x00);
    io_outb(0x3FB, 0x03);
    io_outb(0x3FA, 0x01);
    io_outb(0x3FC, 0x0B);
}

static void serial_putc(char c) {
    if (c == '\n') serial_putc('\r');
    while ((io_inb(0x3FD) & 0x20) == 0) { }
    io_outb(0x3F8, (unsigned char)c);
}

static void serial_puts(const char *s) { while (*s) serial_putc(*s++); }
static void serial_uint(unsigned int v) {
    char buf[12]; int n = 0;
    if (v == 0) buf[n++] = '0';
    while (v) { buf[n++] = '0' + (v % 10); v /= 10; }
    for (int i = n - 1; i >= 0; i--) serial_putc(buf[i]);
}
static void serial_hex(unsigned int v, int digits) {
    static const char hex[] = "0123456789abcdef";
    for (int i = digits - 1; i >= 0; i--) serial_putc(hex[(v >> (i*4)) & 0xF]);
}

/* =====================================================================
 * Info VBE (la deja el bootloader en 0x6000)
 * ===================================================================== */
#define VBE_MAGIC 0x31454256

typedef struct {
    unsigned int  magic;
    unsigned int  fb;
    unsigned short pitch;
    unsigned short width;
    unsigned short height;
    unsigned char  bpp;
} VbeInfo;

static VbeInfo vbe;
static int  fb_ready     = 0;
static unsigned char *fb  = 0;
static int  fb_w          = 0;
static int  fb_h          = 0;
static int  fb_pitch      = 0;
static int  fb_bytes_pp   = 0;

static void vbe_init(void) {
    vbe = *(VbeInfo *)0x6000;
    if (vbe.magic != VBE_MAGIC) { fb_ready = 0; return; }
    fb          = (unsigned char *)vbe.fb;
    fb_w        = vbe.width;
    fb_h        = vbe.height;
    fb_pitch    = vbe.pitch;
    fb_bytes_pp = vbe.bpp / 8;
    if (fb_bytes_pp != 4 && fb_bytes_pp != 3) { fb_ready = 0; return; }
    fb_ready = 1;
}

/* =====================================================================
 * Dibujo
 * ===================================================================== */
static inline void putpixel(int x, int y, unsigned int color) {
    if (x < 0 || x >= fb_w || y < 0 || y >= fb_h) return;
    unsigned char *p = fb + (unsigned int)y * fb_pitch + (unsigned int)x * fb_bytes_pp;
    p[0] = (color      ) & 0xFF;
    p[1] = (color >>  8) & 0xFF;
    p[2] = (color >> 16) & 0xFF;
    if (fb_bytes_pp == 4) p[3] = 0;
}

static void fill_rect(int x, int y, int w, int h, unsigned int color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fb_w) w = fb_w - x;
    if (y + h > fb_h) h = fb_h - y;
    if (w <= 0 || h <= 0) return;
    for (int row = 0; row < h; row++)
        for (int col = 0; col < w; col++)
            putpixel(x + col, y + row, color);
}

static void draw_char(int x, int y, char c, unsigned int fg, unsigned int bg) {
    const uint8_t *glyph = font8x16[(unsigned char)c];
    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            unsigned int color = (bits & (0x80 >> col)) ? fg : bg;
            putpixel(x + col, y + row, color);
        }
    }
}

/* =====================================================================
 * Consola de texto sobre el framebuffer
 * ===================================================================== */
#define CONSOLE_COLS 80
#define CONSOLE_ROWS 25
#define CHAR_W 8
#define CHAR_H 16

static int con_row = 0;
static int con_col = 0;

static void con_clear(void) {
    fill_rect(0, 0, fb_w, fb_h, 0x00000000);
    con_row = 0; con_col = 0;
}

static void con_putc(char c) {
    if (c == '\n') { con_col = 0; con_row++; }
    else {
        draw_char(con_col * CHAR_W, con_row * CHAR_H, c, 0x00FFFFFF, 0x00000000);
        con_col++;
        if (con_col >= CONSOLE_COLS) { con_col = 0; con_row++; }
    }
    if (con_row >= CONSOLE_ROWS) {
        int line_bytes = fb_pitch * CHAR_H;
        unsigned char *dst = fb;
        unsigned char *src = fb + line_bytes;
        for (int i = 0; i < line_bytes * (CONSOLE_ROWS - 1); i++) dst[i] = src[i];
        fill_rect(0, (CONSOLE_ROWS - 1) * CHAR_H, fb_w, CHAR_H, 0x00000000);
        con_row = CONSOLE_ROWS - 1;
    }
}

static void con_puts(const char *s) { while (*s) con_putc(*s++); }

static void con_uint(unsigned int v) {
    char buf[12]; int n = 0;
    if (v == 0) buf[n++] = '0';
    while (v) { buf[n++] = '0' + (v % 10); v /= 10; }
    for (int i = n - 1; i >= 0; i--) con_putc(buf[i]);
}

static void con_hex(unsigned int v, int digits) {
    static const char hex[] = "0123456789abcdef";
    for (int i = digits - 1; i >= 0; i--) con_putc(hex[(v >> (i*4)) & 0xF]);
}

/* Imprime en pantalla Y en serie a la vez */
static void dup_puts(const char *s) { con_puts(s); serial_puts(s); }
static void dup_uint(unsigned int v) { con_uint(v); serial_uint(v); }
static void dup_hex(unsigned int v, int digits) { con_hex(v, digits); serial_hex(v, digits); }

/* =====================================================================
 * kernel_main
 * ===================================================================== */
extern "C" void kernel_main(void) {
    serial_init();

    dup_puts("\n\n================================\n");
    dup_puts(" TEST AHCI automatico\n");
    dup_puts("================================\n\n");

    gdt_init();
    tss_init();
    paging_init();
    paging_enable_now();
    heap_init();

    dup_puts("[+] Init basico OK\n");

    /* Intentar init del framebuffer VBE */
    vbe_init();
    if (fb_ready) {
        con_clear();
        dup_puts("[+] VBE: framebuffer OK\n");
        dup_puts("    fb=");  dup_hex(vbe.fb, 8);
        dup_puts(" pitch="); dup_uint(vbe.pitch);
        dup_puts(" w=");     dup_uint(vbe.width);
        dup_puts(" h=");     dup_uint(vbe.height);
        dup_puts(" bpp=");   dup_uint(vbe.bpp);
        dup_puts("\n");
    } else {
        dup_puts("[!] VBE no disponible (pantalla en negro)\n");
    }

    dup_puts("\n[+] Llamando a ahci_init()...\n\n");

    int r = ahci_init();

    dup_puts("\n================================\n");
    if (r) {
        dup_puts(" RESULTADO: AHCI LISTO\n");
        dup_puts(" Sector count: ");
        dup_uint((unsigned int)ahci_get_sector_count());
        dup_puts("\n");
    } else {
        dup_puts(" RESULTADO: AHCI NO DISPONIBLE\n");
    }
    dup_puts("================================\n\n");

    /* Si tenemos framebuffer, repintar el log del AHCI completo */
    if (fb_ready) {
        con_puts("=== AHCI LOG ===\n");
        int n = ahci_log_count_get();
        for (int i = 0; i < n; i++) {
            const char *line = ahci_log_get(i);
            if (line) { con_puts(line); con_puts("\n"); }
        }
        con_puts("\n=== FIN ===\n");
    }

    /* Repintar el log tambien por serie */
    serial_puts("\n=== AHCI LOG (serie) ===\n");
    int n = ahci_log_count_get();
    for (int i = 0; i < n; i++) {
        const char *line = ahci_log_get(i);
        if (line) { serial_puts(line); serial_puts("\n"); }
    }
    serial_puts("\n=== FIN ===\n");

    dup_puts("\nBucle infinito. Reinicia para salir.\n");

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
