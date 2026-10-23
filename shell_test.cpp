// shell_test.cpp
// Shell minima de prueba: VBE framebuffer + poll directo del teclado (0x60).
// SIN IDT, SIN ISR, SIN IRQ. Todo autocontenido.

#include "shell_test.h"
#include "ahci.h"

/* =====================================================================
 * Fuente 8x16
 * ===================================================================== */
extern "C" uint8_t font8x16[256][16];

/* =====================================================================
 * Puertos I/O inline
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

static int  fb_ready    = 0;
static unsigned char *fb = 0;
static int  fb_w         = 0;
static int  fb_h         = 0;
static int  fb_pitch     = 0;
static int  fb_bytes_pp  = 0;

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
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            putpixel(x + col, y + row, color);
        }
    }
}

static void draw_char(int x, int y, char c, unsigned int fg, unsigned int bg) {
    unsigned char uc = (unsigned char)c;
    const uint8_t *glyph = font8x16[uc];
    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            unsigned int color = (bits & (0x80 >> col)) ? fg : bg;
            putpixel(x + col, y + row, color);
        }
    }
}

/* =====================================================================
 * Consola de texto
 * ===================================================================== */
#define CONSOLE_COLS 80
#define CONSOLE_ROWS 25
#define CHAR_W 8
#define CHAR_H 16

#define COL_FG   0x00FFFFFF
#define COL_BG   0x00000000

static int con_row = 0;
static int con_col = 0;

static void console_clear(void) {
    if (!fb_ready) return;
    fill_rect(0, 0, fb_w, fb_h, COL_BG);
    con_row = 0;
    con_col = 0;
}

static void console_putc(char c) {
    if (!fb_ready) return;

    if (c == '\n') { con_col = 0; con_row++; }
    else if (c == '\r') { con_col = 0; }
    else if (c == '\b') {
        if (con_col > 0) {
            con_col--;
            draw_char(con_col * CHAR_W, con_row * CHAR_H, ' ', COL_FG, COL_BG);
        }
        return;
    } else {
        draw_char(con_col * CHAR_W, con_row * CHAR_H, c, COL_FG, COL_BG);
        con_col++;
        if (con_col >= CONSOLE_COLS) { con_col = 0; con_row++; }
    }

    if (con_row >= CONSOLE_ROWS) {
        int line_bytes = fb_pitch * CHAR_H;
        unsigned char *dst = fb;
        unsigned char *src = fb + line_bytes;
        for (int i = 0; i < line_bytes * (CONSOLE_ROWS - 1); i++) dst[i] = src[i];
        fill_rect(0, (CONSOLE_ROWS - 1) * CHAR_H, fb_w, CHAR_H, COL_BG);
        con_row = CONSOLE_ROWS - 1;
    }
}

static void console_puts(const char *s) {
    while (*s) console_putc(*s++);
}

static void console_uint(unsigned int v) {
    char buf[12]; int n = 0;
    if (v == 0) buf[n++] = '0';
    while (v) { buf[n++] = '0' + (v % 10); v /= 10; }
    for (int i = n - 1; i >= 0; i--) console_putc(buf[i]);
}

/* =====================================================================
 * Poll directo del teclado (sin IRQ)
 * ===================================================================== */
static const char kbd_us[128] = {
    0, 27,
    '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,
    '\\','z','x','c','v','b','n','m',',','.','/',
    0,'*',0,' ',
    0,
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,
    0,0,0,
    0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static char kbd_poll_getchar(void) {
    /* ¿Hay dato disponible? status bit 0 */
    unsigned char status = io_inb(0x64);
    if (!(status & 1)) return 0;

    /* Leer scancode */
    unsigned char sc = io_inb(0x60);

    /* Ignorar releases */
    if (sc & 0x80) return 0;

    if (sc >= 128) return 0;
    char c = kbd_us[sc];
    return c;
}

/* =====================================================================
 * Helpers de string
 * ===================================================================== */
static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

/* =====================================================================
 * Comandos
 * ===================================================================== */
static void cmd_help(void) {
    console_puts("Comandos disponibles:\n");
    console_puts("  help     - esta ayuda\n");
    console_puts("  disk     - estado del disco AHCI\n");
    console_puts("  ahcilog  - log interno del driver AHCI\n");
    console_puts("  vbe      - info del framebuffer VBE\n");
    console_puts("  clear    - limpiar pantalla\n");
}

static void cmd_vbe(void) {
    console_puts("=== VBE INFO ===\n");
    console_puts("Magic:     ");  console_uint(vbe.magic);
    console_puts("\nFramebuffer: 0x");
    {
        char hex[12]; int n = 0;
        unsigned int v = vbe.fb;
        if (v == 0) hex[n++] = '0';
        while (v) {
            unsigned int d = v & 0xF;
            hex[n++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
            v >>= 4;
        }
        for (int i = n - 1; i >= 0; i--) console_putc(hex[i]);
    }
    console_puts("\nPitch:     "); console_uint(vbe.pitch);
    console_puts("\nWidth:     "); console_uint(vbe.width);
    console_puts("\nHeight:    "); console_uint(vbe.height);
    console_puts("\nBPP:       "); console_uint(vbe.bpp);
    console_puts("\n");
}

static void cmd_disk(void) {
    console_puts("=== DISCO AHCI ===\n");

    if (!ahci_is_ready()) {
        console_puts("AHCI: no disponible\n");
        return;
    }

    unsigned int count = (unsigned int)ahci_get_sector_count();
    unsigned int mb = (unsigned int)((ahci_get_sector_count() * 512ULL) / (1024ULL * 1024ULL));

    console_puts("Estado:    LISTO\n");
    console_puts("Sectores:  "); console_uint(count);
    console_puts("\nCapacidad: "); console_uint(mb);
    console_puts(" MiB\n");

    static unsigned char buf[512];
    if (!ahci_read_sector(0, buf)) {
        console_puts("Error al leer sector 0\n");
        return;
    }

    console_puts("MBR (primeros 16 bytes):\n");
    for (int i = 0; i < 16; i++) {
        char hex[4];
        unsigned char b = buf[i];
        hex[0] = "0123456789ABCDEF"[(b >> 4) & 0xF];
        hex[1] = "0123456789ABCDEF"[b & 0xF];
        hex[2] = ' ';
        hex[3] = 0;
        console_puts(hex);
    }
    console_puts("\n");

    if (buf[510] == 0x55 && buf[511] == 0xAA) {
        console_puts("Firma 0x55AA: OK (hay MBR)\n");
    } else {
        console_puts("Firma 0x55AA: NO (disco sin MBR)\n");
    }
}

static void cmd_ahcilog(void) {
    console_puts("=== AHCI LOG ===\n");

    int n = ahci_log_count_get();
    if (n == 0) { console_puts("(sin log)\n"); return; }
    for (int i = 0; i < n; i++) {
        const char *line = ahci_log_get(i);
        if (line) {
            console_puts(line);
            console_puts("\n");
        }
    }
}

/* =====================================================================
 * Prompt
 * ===================================================================== */
#define LINE_MAX 64
static char line[LINE_MAX];
static int  line_len = 0;

static void prompt(void) { console_puts("> "); }

static void execute(const char *cmd) {
    if (cmd[0] == 0) return;

    if (str_eq(cmd, "help"))         cmd_help();
    else if (str_eq(cmd, "disk"))    cmd_disk();
    else if (str_eq(cmd, "ahcilog")) cmd_ahcilog();
    else if (str_eq(cmd, "vbe"))     cmd_vbe();
    else if (str_eq(cmd, "clear"))   console_clear();
    else {
        console_puts("Desconocido: ");
        console_puts(cmd);
        console_puts("\n");
    }
}

/* =====================================================================
 * API publica
 * ===================================================================== */
extern "C" void shell_test_run(void) {
    vbe_init();

    if (!fb_ready) {
        while (1) { __asm__ __volatile__("hlt"); }
    }

    console_clear();

    console_puts("========================================\n");
    console_puts(" MiniSO - Shell de prueba AHCI\n");
    console_puts("========================================\n\n");
    console_puts("Escribe 'help' para ver comandos.\n\n");

    line_len = 0;
    line[0] = 0;
    prompt();

    while (1) {
        char c = kbd_poll_getchar();
        if (c == 0) {
            __asm__ __volatile__("pause");
            continue;
        }

        if (c == '\n') {
            console_putc('\n');
            line[line_len] = 0;
            execute(line);
            line_len = 0;
            line[0] = 0;
            prompt();
        }
        else if (c == '\b') {
            if (line_len > 0) { line_len--; console_putc('\b'); }
        }
        else if (c >= 32 && c < 127) {
            if (line_len < LINE_MAX - 1) {
                line[line_len++] = c;
                console_putc(c);
            }
        }
    }
}
