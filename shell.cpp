#include "shell.h"
#include "screen.h"
#include "keyboard.h"
#include "pit.h"
#include "io.h"
#include "heap.h"
#include "rtc.h"
#include "fs.h"
#include "ahci.h"

#define LINE_MAX 128

static char line[LINE_MAX];
static int line_len = 0;

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
// Comandos basicos
// =====================================================================
static void cmd_help() {
    screen_write("Comandos disponibles:\n");
    screen_write("  help          - esta ayuda\n");
    screen_write("  clear         - limpiar pantalla\n");
    screen_write("  ticks         - ticks desde el arranque\n");
    screen_write("  uptime        - tiempo desde el arranque\n");
    screen_write("  date          - fecha y hora del RTC\n");
    screen_write("  echo <texto>  - imprime el texto\n");
    screen_write("  cpuid         - informacion del CPU\n");
    screen_write("  mem           - memoria RAM detectada\n");
    screen_write("  heap          - estadisticas del heap\n");
    screen_write("  test          - prueba de kmalloc/kfree\n");
    screen_write("  ls            - listar archivos\n");
    screen_write("  touch <name>  - crear archivo\n");
    screen_write("  write <a> <t> - escribir texto\n");
    screen_write("  read <a>      - leer archivo\n");
    screen_write("  rm <a>        - borrar archivo\n");
    screen_write("  diskinfo      - info del disco SATA\n");
    screen_write("  diskread <lba> - leer sector del disco (hex)\n");
    screen_write("  reboot        - reiniciar el PC\n");
    screen_write("  shutdown      - apagar el PC\n");
}

static void cmd_clear()   { screen_clear(); }
static void cmd_ticks()   { kprintf("Ticks: %u\n", pit_get_ticks()); }

static void cmd_uptime() {
    unsigned int t = pit_get_ticks() / 100;
    unsigned int h = t / 3600;
    unsigned int m = (t % 3600) / 60;
    unsigned int s = t % 60;
    kprintf("Uptime: %02u:%02u:%02u\n", h, m, s);
}

static void cmd_date() {
    RTCTime t;
    rtc_read(&t);
    kprintf("Fecha: %02u/%02u/%u\n", t.day, t.month, t.year);
    kprintf("Hora:  %02u:%02u:%02u\n", t.hour, t.minute, t.second);
}

static void cmd_echo(const char *args) {
    screen_write(args);
    screen_write("\n");
}

// =====================================================================
// CPUID
// =====================================================================
static void cpuid_raw(unsigned int leaf, unsigned int subleaf,
                      unsigned int *eax, unsigned int *ebx,
                      unsigned int *ecx, unsigned int *edx) {
    asm volatile("cpuid"
                 : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                 : "a"(leaf), "c"(subleaf));
}

static void cmd_cpuid() {
    unsigned int eax, ebx, ecx, edx;

    cpuid_raw(0, 0, &eax, &ebx, &ecx, &edx);
    unsigned int max_leaf = eax;

    char vendor[13];
    vendor[0] = ebx & 0xFF;  vendor[1] = (ebx >> 8) & 0xFF;
    vendor[2] = (ebx >> 16) & 0xFF; vendor[3] = (ebx >> 24) & 0xFF;
    vendor[4] = edx & 0xFF;  vendor[5] = (edx >> 8) & 0xFF;
    vendor[6] = (edx >> 16) & 0xFF; vendor[7] = (edx >> 24) & 0xFF;
    vendor[8] = ecx & 0xFF;  vendor[9] = (ecx >> 8) & 0xFF;
    vendor[10] = (ecx >> 16) & 0xFF; vendor[11] = (ecx >> 24) & 0xFF;
    vendor[12] = 0;

    kprintf("CPU fabricante: %s\n", vendor);
    kprintf("Hoja maxima: %u\n", max_leaf);

    if (max_leaf >= 1) {
        cpuid_raw(1, 0, &eax, &ebx, &ecx, &edx);

        unsigned int stepping   = eax & 0xF;
        unsigned int model      = (eax >> 4) & 0xF;
        unsigned int family     = (eax >> 8) & 0xF;
        unsigned int ext_model  = (eax >> 16) & 0xF;
        unsigned int ext_family = (eax >> 20) & 0xFF;
        unsigned int brand_id   = ebx & 0xFF;
        unsigned int clflush    = ((ebx >> 8) & 0xFF) * 8;
        unsigned int logical    = (ebx >> 16) & 0xFF;

        if (family == 0xF) family += ext_family;
        if (family == 0x6 || family == 0xF) model += (ext_model << 4);

        kprintf("Familia: %u  Modelo: %u  Stepping: %u\n",
                family, model, stepping);
        kprintf("Cores logicos: %u\n", logical);
        kprintf("Cache line: %u bytes\n", clflush);
        kprintf("Brand ID: %u\n", brand_id);
    }

    cpuid_raw(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    unsigned int max_ext = eax;
    kprintf("Hoja extendida maxima: %x\n", max_ext);

    if (max_ext >= 0x80000004) {
        char brand[49];
        unsigned int *p = (unsigned int*)brand;
        cpuid_raw(0x80000002, 0, &p[0], &p[1], &p[2], &p[3]);
        cpuid_raw(0x80000003, 0, &p[4], &p[5], &p[6], &p[7]);
        cpuid_raw(0x80000004, 0, &p[8], &p[9], &p[10], &p[11]);
        brand[48] = 0;
        kprintf("Nombre: %s\n", brand);
    }
}

// =====================================================================
// MEM
// =====================================================================
struct E820Entry {
    unsigned long long base;
    unsigned long long length;
    unsigned int type;
    unsigned int acpi;
} __attribute__((packed));

static void cmd_mem() {
    unsigned int num = *(volatile unsigned int*)0x5000;
    if (num == 0 || num > 64) {
        screen_write("Mapa E820 no disponible.\n");
        return;
    }

    kprintf("Entradas E820: %u\n", num);
    unsigned long long total_usable = 0;

    volatile E820Entry *entries = (volatile E820Entry*)0x5004;
    for (unsigned int i = 0; i < num; i++) {
        unsigned long long base = entries[i].base;
        unsigned long long len  = entries[i].length;
        unsigned int type       = entries[i].type;

        const char *tname;
        if (type == 1)      tname = "Usable";
        else if (type == 2) tname = "Reservada";
        else if (type == 3) tname = "ACPI";
        else if (type == 4) tname = "NVS";
        else if (type == 5) tname = "Unusable";
        else                tname = "Otro";

        kprintf("  [%u] base=%08x%08x len=%08x%08x %s\n",
                i,
                (unsigned int)(base >> 32), (unsigned int)(base & 0xFFFFFFFF),
                (unsigned int)(len >> 32),  (unsigned int)(len & 0xFFFFFFFF),
                tname);

        if (type == 1) total_usable += len;
    }

    unsigned long long total_mb = total_usable / (1024ULL * 1024ULL);
    kprintf("RAM usable total: %u MB\n", (unsigned int)total_mb);
}

// =====================================================================
// HEAP
// =====================================================================
static void cmd_heap() {
    HeapStats s = heap_get_stats();
    kprintf("Heap total:   %u KB\n", s.total_size / 1024);
    kprintf("Heap usado:   %u KB\n", s.used_size / 1024);
    kprintf("Heap libre:   %u KB\n", s.free_size / 1024);
    kprintf("Bloques:      %u total, %u libres\n", s.num_blocks, s.num_free);
}

static void cmd_test() {
    kprintf("Asignando 3 bloques...\n");
    void *a = kmalloc(1024);
    void *b = kmalloc(2048);
    void *c = kmalloc(4096);
    kprintf("  a=%x  b=%x  c=%x\n",
            (unsigned int)a, (unsigned int)b, (unsigned int)c);

    kfree(b);
    kfree(a);
    kfree(c);

    HeapStats s3 = heap_get_stats();
    kprintf("Despues de liberar todo: %u KB usados, %u bloques, %u libres\n",
            s3.used_size / 1024, s3.num_blocks, s3.num_free);
}

// =====================================================================
// FS
// =====================================================================
static void cmd_ls() { fs_list(); }

static void cmd_touch(const char *args) {
    if (args[0] == 0) { screen_write("Uso: touch <nombre>\n"); return; }
    int r = fs_create(args);
    if (r < 0) kprintf("Error: %d\n", r);
    else       kprintf("Creado: %s\n", args);
}

static void cmd_write(const char *args) {
    if (args[0] == 0) { screen_write("Uso: write <archivo> <texto>\n"); return; }

    char name[64];
    int i = 0;
    while (args[i] && args[i] != ' ' && i < 63) { name[i] = args[i]; i++; }
    name[i] = 0;
    if (args[i] == ' ') i++;
    const char *text = args + i;

    int fd = fs_open(name);
    if (fd < 0) { kprintf("Error open: %d\n", fd); return; }

    fs_seek(fd, fs_size(fd));

    int len = 0;
    while (text[len]) len++;

    int w = fs_write(fd, text, len);
    fs_close(fd);

    if (w < 0) kprintf("Error write: %d\n", w);
    else       kprintf("Escrito %d bytes en %s\n", w, name);
}

static void cmd_read(const char *args) {
    if (args[0] == 0) { screen_write("Uso: read <archivo>\n"); return; }

    int fd = fs_open(args);
    if (fd < 0) { kprintf("Error: %d\n", fd); return; }

    int size = fs_size(fd);
    if (size <= 0) {
        kprintf("%s (vacio)\n", args);
        fs_close(fd);
        return;
    }

    static char buf[FS_MAX_SIZE + 1];
    int n = fs_read(fd, buf, FS_MAX_SIZE);
    fs_close(fd);

    buf[n] = 0;
    kprintf("=== %s (%d bytes) ===\n", args, n);
    screen_write(buf);
    screen_write("\n");
}

static void cmd_rm(const char *args) {
    if (args[0] == 0) { screen_write("Uso: rm <archivo>\n"); return; }
    int r = fs_delete(args);
    if (r < 0) kprintf("Error: %d\n", r);
    else       kprintf("Borrado: %s\n", args);
}

// =====================================================================
// DISCO: comandos AHCI
// =====================================================================
static void cmd_diskinfo() {
    if (!ahci_is_ready()) {
        screen_write("AHCI no disponible\n");
        return;
    }

    uint64_t sectors = ahci_get_sector_count();
    uint32_t mib = (uint32_t)(sectors / 2048);

    kprintf("AHCI: driver listo\n");
    kprintf("Sectores totales: %u:%u\n",
            (uint32_t)(sectors >> 32),
            (uint32_t)(sectors));
    kprintf("Capacidad: %u MiB\n", mib);
}

static void cmd_diskread(const char *args) {
    if (!ahci_is_ready()) {
        screen_write("AHCI no disponible\n");
        return;
    }

    if (args[0] == 0) {
        screen_write("Uso: diskread <lba>\n");
        return;
    }

    uint32_t lba = 0;
    for (int i = 0; args[i] >= '0' && args[i] <= '9'; i++) {
        lba = lba * 10 + (args[i] - '0');
    }

    static unsigned char buf[512];
    if (!ahci_read_sector((uint64_t)lba, buf)) {
        kprintf("Error leyendo sector %u\n", lba);
        return;
    }

    kprintf("Sector %u (LBA):\n", lba);
    kprintf("  Primeros 64 bytes:\n");

    for (int row = 0; row < 4; row++) {
        // Offset en hex, con padding manual (kprintf no soporta %04x)
        screen_write("  ");
        const char *hex = "0123456789ABCDEF";
        unsigned int off = row * 16;
        screen_putchar(hex[(off >> 12) & 0xF]);
        screen_putchar(hex[(off >> 8) & 0xF]);
        screen_putchar(hex[(off >> 4) & 0xF]);
        screen_putchar(hex[off & 0xF]);
        screen_write(": ");

        // Hex
        for (int col = 0; col < 16; col++) {
            unsigned char b = buf[row * 16 + col];
            screen_putchar(hex[(b >> 4) & 0xF]);
            screen_putchar(hex[b & 0xF]);
            screen_putchar(' ');
        }

        screen_write(" | ");

        // ASCII
        for (int col = 0; col < 16; col++) {
            unsigned char b = buf[row * 16 + col];
            if (b >= 32 && b < 127) screen_putchar((char)b);
            else                    screen_putchar('.');
        }
        screen_write("\n");
    }
}

// =====================================================================
// REBOOT / SHUTDOWN
// =====================================================================
static void cmd_reboot() {
    screen_write("Reiniciando...\n");
    while (inb(0x64) & 1) inb(0x60);
    outb(0x64, 0xFE);
    asm volatile("cli");
    asm volatile("lidt (0)");
    asm volatile("int $0x03");
    while (1) {}
}

static void cmd_shutdown() {
    screen_write("Apagando...\n");
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    screen_write("Shutdown no soportado.\n");
}

// =====================================================================
// Dispatcher
// =====================================================================
static void execute(const char *l) {
    if (str_eq(l, "help"))              cmd_help();
    else if (str_eq(l, "clear"))        cmd_clear();
    else if (str_eq(l, "ticks"))        cmd_ticks();
    else if (str_eq(l, "uptime"))       cmd_uptime();
    else if (str_eq(l, "date"))         cmd_date();
    else if (str_eq(l, "cpuid"))        cmd_cpuid();
    else if (str_eq(l, "mem"))          cmd_mem();
    else if (str_eq(l, "heap"))         cmd_heap();
    else if (str_eq(l, "test"))         cmd_test();
    else if (str_eq(l, "ls"))           cmd_ls();
    else if (str_eq(l, "diskinfo"))     cmd_diskinfo();
    else if (str_eq(l, "reboot"))       cmd_reboot();
    else if (str_eq(l, "shutdown"))     cmd_shutdown();
    else if (str_starts_with(l, "echo "))     cmd_echo(l + 5);
    else if (str_starts_with(l, "touch "))    cmd_touch(l + 6);
    else if (str_starts_with(l, "write "))    cmd_write(l + 6);
    else if (str_starts_with(l, "read "))     cmd_read(l + 5);
    else if (str_starts_with(l, "rm "))       cmd_rm(l + 3);
    else if (str_starts_with(l, "diskread ")) cmd_diskread(l + 9);
    else if (l[0] == 0) { /* linea vacia */ }
    else kprintf("Comando desconocido: %s\n", l);
}

// =====================================================================
// Bucle de la shell
// =====================================================================
void shell_run() {
    kprintf("> ");
    line_len = 0;

    while (1) {
        char c = keyboard_getchar();
        if (!c) continue;

        if (c == '\n') {
            screen_putchar('\n');
            line[line_len] = 0;
            execute(line);
            line_len = 0;
            kprintf("> ");
        } else if (c == '\b') {
            if (line_len > 0) {
                line_len--;
                screen_putchar('\b');
            }
        } else if (c >= 32 && c < 127) {
            if (line_len < LINE_MAX - 1) {
                line[line_len++] = c;
                screen_putchar(c);
            }
        }
    }
}
