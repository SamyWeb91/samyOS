#include "shell.h"
#include "screen.h"
#include "keyboard.h"
#include "pit.h"
#include "io.h"
#include "heap.h"

#define LINE_MAX 128

static char line[LINE_MAX];
static int line_len = 0;

// =====================================================================
// Helpers de strings
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
// Comandos
// =====================================================================
static void cmd_help() {
    screen_write("Comandos disponibles:\n");
    screen_write("  help          - esta ayuda\n");
    screen_write("  clear         - limpiar pantalla\n");
    screen_write("  ticks         - ticks desde el arranque\n");
    screen_write("  uptime        - tiempo desde el arranque\n");
    screen_write("  echo <texto>  - imprime el texto\n");
    screen_write("  cpuid         - informacion del CPU\n");
    screen_write("  mem           - memoria RAM detectada\n");
    screen_write("  heap          - estadisticas del heap\n");
    screen_write("  test          - prueba de kmalloc/kfree\n");
    screen_write("  reboot        - reiniciar el PC\n");
    screen_write("  shutdown      - apagar el PC\n");
}

static void cmd_clear() {
    screen_clear();
}

static void cmd_ticks() {
    kprintf("Ticks: %u\n", pit_get_ticks());
}

static void cmd_uptime() {
    unsigned int t = pit_get_ticks() / 100;
    unsigned int h = t / 3600;
    unsigned int m = (t % 3600) / 60;
    unsigned int s = t % 60;
    kprintf("Uptime: %02u:%02u:%02u\n", h, m, s);
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
        unsigned int apic_id    = (ebx >> 24) & 0xFF;

        if (family == 0xF) family += ext_family;
        if (family == 0x6 || family == 0xF) model += (ext_model << 4);

        kprintf("Familia: %u  Modelo: %u  Stepping: %u\n",
                family, model, stepping);
        kprintf("Cores logicos: %u\n", logical);
        kprintf("APIC ID: %u\n", apic_id);
        kprintf("Cache line: %u bytes\n", clflush);
        kprintf("Brand ID: %u\n", brand_id);

        kprintf("Flags: ");
        if (edx & (1 << 0))  kprintf("FPU ");
        if (edx & (1 << 4))  kprintf("TSC ");
        if (edx & (1 << 5))  kprintf("MSR ");
        if (edx & (1 << 6))  kprintf("PAE ");
        if (edx & (1 << 9))  kprintf("APIC ");
        if (edx & (1 << 23)) kprintf("MMX ");
        if (edx & (1 << 25)) kprintf("SSE ");
        if (edx & (1 << 26)) kprintf("SSE2 ");
        if (ecx & (1 << 0))  kprintf("SSE3 ");
        if (ecx & (1 << 9))  kprintf("SSSE3 ");
        if (ecx & (1 << 19)) kprintf("SSE4.1 ");
        if (ecx & (1 << 20)) kprintf("SSE4.2 ");
        if (ecx & (1 << 28)) kprintf("AVX ");
        if (ecx & (1 << 29)) kprintf("F16C ");
        if (ecx & (1 << 30)) kprintf("RDRAND ");
        kprintf("\n");
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

    if (max_ext >= 0x80000006) {
        cpuid_raw(0x80000006, 0, &eax, &ebx, &ecx, &edx);
        unsigned int l2_size = (ecx >> 16) & 0xFFFF;
        kprintf("Cache L2: %u KB\n", l2_size);
    }

    if (max_ext >= 0x80000007) {
        cpuid_raw(0x80000007, 0, &eax, &ebx, &ecx, &edx);
        if (edx & (1 << 8)) {
            kprintf("TSC invariante: si\n");
        } else {
            kprintf("TSC invariante: no\n");
        }
    }
}

// =====================================================================
// MEM: leer mapa E820 guardado por el bootloader en 0x5000
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

// =====================================================================
// TEST: prueba de kmalloc/kfree
// =====================================================================
static void cmd_test() {
    kprintf("Asignando 3 bloques...\n");

    void *a = kmalloc(1024);
    void *b = kmalloc(2048);
    void *c = kmalloc(4096);

    kprintf("  a=%x  b=%x  c=%x\n",
            (unsigned int)a, (unsigned int)b, (unsigned int)c);

    HeapStats s1 = heap_get_stats();
    kprintf("Despues de asignar: %u KB usados, %u bloques\n",
            s1.used_size / 1024, s1.num_blocks);

    kprintf("Liberando b...\n");
    kfree(b);

    HeapStats s2 = heap_get_stats();
    kprintf("Despues de liberar b: %u KB usados, %u bloques, %u libres\n",
            s2.used_size / 1024, s2.num_blocks, s2.num_free);

    kprintf("Liberando a y c...\n");
    kfree(a);
    kfree(c);

    HeapStats s3 = heap_get_stats();
    kprintf("Despues de liberar todo: %u KB usados, %u bloques, %u libres\n",
            s3.used_size / 1024, s3.num_blocks, s3.num_free);
}

// =====================================================================
// REBOOT
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

// =====================================================================
// SHUTDOWN
// =====================================================================
static void cmd_shutdown() {
    screen_write("Apagando...\n");
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    screen_write("Shutdown no soportado en este hardware.\n");
}

// =====================================================================
// Dispatcher de comandos
// =====================================================================
static void execute(const char *l) {
    if (str_eq(l, "help")) {
        cmd_help();
    } else if (str_eq(l, "clear")) {
        cmd_clear();
    } else if (str_eq(l, "ticks")) {
        cmd_ticks();
    } else if (str_eq(l, "uptime")) {
        cmd_uptime();
    } else if (str_eq(l, "cpuid")) {
        cmd_cpuid();
    } else if (str_eq(l, "mem")) {
        cmd_mem();
    } else if (str_eq(l, "heap")) {
        cmd_heap();
    } else if (str_eq(l, "test")) {
        cmd_test();
    } else if (str_eq(l, "reboot")) {
        cmd_reboot();
    } else if (str_eq(l, "shutdown")) {
        cmd_shutdown();
    } else if (str_starts_with(l, "echo ")) {
        cmd_echo(l + 5);
    } else if (l[0] == 0) {
        // linea vacia
    } else {
        kprintf("Comando desconocido: %s\n", l);
    }
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
