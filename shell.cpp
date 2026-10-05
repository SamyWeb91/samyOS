#include "shell.h"
#include "screen.h"
#include "keyboard.h"
#include "pit.h"
#include "io.h"
#include "heap.h"
#include "rtc.h"
#include "fs.h"
#include "ahci.h"
#include "thread.h"
#include "tss.h"
#include "user.h"

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

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
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
    screen_write("  free          - memoria libre\n");
    screen_write("  test          - prueba de kmalloc/kfree\n");
    screen_write("  tssinfo       - info del TSS\n");
    screen_write("  ls            - listar archivos\n");
    screen_write("  df            - espacio del FS\n");
    screen_write("  touch <name>  - crear archivo\n");
    screen_write("  write <a> <t> - escribir texto\n");
    screen_write("  append <a> <t> - anadir texto al final\n");
    screen_write("  read <a>      - leer archivo\n");
    screen_write("  cat <a>       - mostrar archivo\n");
    screen_write("  cp <a> <b>    - copiar archivo\n");
    screen_write("  mv <a> <b>    - renombrar archivo\n");
    screen_write("  rm <a>        - borrar archivo\n");
    screen_write("  wc <a>        - contar lineas/palabras/bytes\n");
    screen_write("  stat <a>      - info del archivo\n");
    screen_write("  find <txt>    - buscar texto en todos los archivos\n");
    screen_write("  grep <txt> <a> - buscar texto en un archivo\n");
    screen_write("  head <a> <n>  - primeras N lineas\n");
    screen_write("  tail <a> <n>  - ultimas N lineas\n");
    screen_write("  diskinfo      - info del disco SATA\n");
    screen_write("  diskread <lba> - leer sector del disco\n");
    screen_write("  diskwrite <lba> <txt> - escribir sector\n");
    screen_write("  ring3         - lanzar proceso en ring 3\n");
    screen_write("  ps            - listar procesos\n");
    screen_write("  kill <pid>    - terminar proceso\n");
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
// TSS info
// =====================================================================
static void cmd_tssinfo() {
    uint16_t tr;
    asm volatile("str %0" : "=r"(tr));

    uint32_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));

    uint16_t cs, ss;
    asm volatile("mov %%cs, %0" : "=r"(cs));
    asm volatile("mov %%ss, %0" : "=r"(ss));

    kprintf("TSS:\n");
    kprintf("  TR    = %x (esperado 0x28)\n", tr);
    kprintf("  base  = %x\n", tss_get_base());
    kprintf("  limit = %u\n",  tss_get_limit());
    kprintf("  ss0   = %x\n", tss_get_ss0());
    kprintf("  esp0  = %x\n", tss_get_esp0());
    kprintf("\n");
    kprintf("CPU:\n");
    kprintf("  CS    = %x (ring %u)\n", cs, cs & 3);
    kprintf("  SS    = %x (ring %u)\n", ss, ss & 3);
    kprintf("  CR3   = %x\n", cr3);
}

// =====================================================================
// Ring 3
//
// ring3_task corre en un hilo nuevo. Cuando el user hace exit,
// thread_exit() mata ESE hilo (no kmain). La shell sigue viva.
// =====================================================================
static void ring3_task(void) {
    // Corremos en ring 0, en un hilo propio.
    // user_start() hace el iret a ring 3.
    // El user hace write+exit. exit mata este hilo.
    user_start();

    // Si user_start() retorna (no debería), matamos el hilo.
    while (1) {
        thread_yield();
    }
}

static void cmd_ring3() {
    kprintf("Creando proceso en ring 3...\n");
    int tid = thread_create("ring3", ring3_task);
    if (tid < 0) {
        kprintf("Error: no se pudo crear el hilo\n");
        return;
    }
    kprintf("Proceso ring3 creado (PID=%d)\n", tid);
    // La shell NO espera. El scheduler correrá ring3_task.
    // Cuando el user haga exit, el hilo muere y volvemos aquí.
}

// =====================================================================
// Helpers para parsear args
// =====================================================================
static int parse_args(const char *args, char *a1, char *a2, int a1max, int a2max) {
    int i = 0;
    while (args[i] && args[i] != ' ' && i < a1max - 1) {
        a1[i] = args[i];
        i++;
    }
    a1[i] = 0;

    if (args[i] == 0) return 1;
    i++;
    int j = 0;
    while (args[i] && j < a2max - 1) {
        a2[j++] = args[i++];
    }
    a2[j] = 0;
    return 2;
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

static void cmd_free() {
    HeapStats s = heap_get_stats();
    kprintf("Memoria libre: %u KB\n", s.free_size / 1024);
    kprintf("Memoria usada: %u KB\n", s.used_size / 1024);
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

static void cmd_df() {
    kprintf("FS: %d archivos max, %d bytes cada uno\n",
            FS_MAX_FILES, FS_MAX_SIZE);
    kprintf("Capacidad total: %u KB\n",
            (FS_MAX_FILES * FS_MAX_SIZE) / 1024);
    kprintf("(usa 'ls' para ver archivos)\n");
}

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

    int len = str_len(text);
    int w = fs_write(fd, text, len);
    fs_close(fd);

    if (w < 0) kprintf("Error write: %d\n", w);
    else       kprintf("Escrito %d bytes en %s\n", w, name);
}

static void cmd_append(const char *args) {
    if (args[0] == 0) { screen_write("Uso: append <archivo> <texto>\n"); return; }

    char name[64];
    int i = 0;
    while (args[i] && args[i] != ' ' && i < 63) { name[i] = args[i]; i++; }
    name[i] = 0;
    if (args[i] == ' ') i++;
    const char *text = args + i;

    int fd = fs_open(name);
    if (fd < 0) { kprintf("Error open: %d\n", fd); return; }

    fs_seek(fd, fs_size(fd));
    int len = str_len(text);
    int w = fs_write(fd, text, len);
    if (w > 0) {
        fs_write(fd, "\n", 1);
        w++;
    }
    fs_close(fd);

    if (w < 0) kprintf("Error write: %d\n", w);
    else       kprintf("Anadidos %d bytes a %s\n", w, name);
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

static void cmd_cat(const char *args) {
    if (args[0] == 0) { screen_write("Uso: cat <archivo>\n"); return; }

    int fd = fs_open(args);
    if (fd < 0) { kprintf("Error: %d\n", fd); return; }

    int size = fs_size(fd);
    if (size <= 0) {
        fs_close(fd);
        return;
    }

    static char buf[FS_MAX_SIZE + 1];
    int n = fs_read(fd, buf, FS_MAX_SIZE);
    fs_close(fd);

    buf[n] = 0;
    screen_write(buf);
    screen_write("\n");
}

static void cmd_cp(const char *args) {
    char src[64], dst[64];
    if (parse_args(args, src, dst, 64, 64) < 2) {
        screen_write("Uso: cp <origen> <destino>\n");
        return;
    }

    int fd_src = fs_open(src);
    if (fd_src < 0) { kprintf("Error abriendo %s: %d\n", src, fd_src); return; }

    int size = fs_size(fd_src);
    if (size <= 0) {
        kprintf("%s (vacio)\n", src);
        fs_close(fd_src);
        return;
    }

    static char buf[FS_MAX_SIZE + 1];
    int n = fs_read(fd_src, buf, FS_MAX_SIZE);
    fs_close(fd_src);

    int fd_dst = fs_create(dst);
    if (fd_dst < 0) { kprintf("Error creando %s: %d\n", dst, fd_dst); return; }

    int w = fs_write(fd_dst, buf, n);
    fs_close(fd_dst);

    if (w < 0) kprintf("Error escribiendo: %d\n", w);
    else       kprintf("Copiados %d bytes de %s a %s\n", w, src, dst);
}

static void cmd_mv(const char *args) {
    char src[64], dst[64];
    if (parse_args(args, src, dst, 64, 64) < 2) {
        screen_write("Uso: mv <origen> <destino>\n");
        return;
    }

    int fd_src = fs_open(src);
    if (fd_src < 0) { kprintf("Error abriendo %s: %d\n", src, fd_src); return; }

    int size = fs_size(fd_src);
    if (size <= 0) {
        kprintf("%s (vacio)\n", src);
        fs_close(fd_src);
        return;
    }

    static char buf[FS_MAX_SIZE + 1];
    int n = fs_read(fd_src, buf, FS_MAX_SIZE);
    fs_close(fd_src);

    int fd_dst = fs_create(dst);
    if (fd_dst < 0) { kprintf("Error creando %s: %d\n", dst, fd_dst); return; }

    fs_write(fd_dst, buf, n);
    fs_close(fd_dst);

    fs_delete(src);

    kprintf("Renombrado %s a %s (%d bytes)\n", src, dst, n);
}

static void cmd_rm(const char *args) {
    if (args[0] == 0) { screen_write("Uso: rm <archivo>\n"); return; }
    int r = fs_delete(args);
    if (r < 0) kprintf("Error: %d\n", r);
    else       kprintf("Borrado: %s\n", args);
}

static void cmd_wc(const char *args) {
    if (args[0] == 0) { screen_write("Uso: wc <archivo>\n"); return; }

    int fd = fs_open(args);
    if (fd < 0) { kprintf("Error: %d\n", fd); return; }

    int size = fs_size(fd);
    if (size <= 0) {
        kprintf("0 0 0 %s\n", args);
        fs_close(fd);
        return;
    }

    static char buf[FS_MAX_SIZE + 1];
    int n = fs_read(fd, buf, FS_MAX_SIZE);
    fs_close(fd);
    buf[n] = 0;

    int lines = 0, words = 0, in_word = 0;
    for (int i = 0; i < n; i++) {
        char c = buf[i];
        if (c == '\n') lines++;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            in_word = 0;
        } else {
            if (!in_word) {
                words++;
                in_word = 1;
            }
        }
    }

    kprintf("%d %d %d %s\n", lines, words, n, args);
}

static void cmd_stat(const char *args) {
    if (args[0] == 0) { screen_write("Uso: stat <archivo>\n"); return; }

    int fd = fs_open(args);
    if (fd < 0) { kprintf("Error: %d\n", fd); return; }

    int size = fs_size(fd);
    fs_close(fd);

    kprintf("Archivo: %s\n", args);
    kprintf("  Tamano:  %d bytes\n", size);
    kprintf("  fd:      %d\n", fd);
}

static void cmd_find(const char *args) {
    if (args[0] == 0) { screen_write("Uso: find <texto>\n"); return; }
    screen_write("find: pendiente de implementar iteracion del FS\n");
}

static void cmd_grep(const char *args) {
    char pattern[64], filename[64];
    if (parse_args(args, pattern, filename, 64, 64) < 2) {
        screen_write("Uso: grep <texto> <archivo>\n");
        return;
    }

    int fd = fs_open(filename);
    if (fd < 0) { kprintf("Error: %d\n", fd); return; }

    int size = fs_size(fd);
    if (size <= 0) { fs_close(fd); return; }

    static char buf[FS_MAX_SIZE + 1];
    int n = fs_read(fd, buf, FS_MAX_SIZE);
    fs_close(fd);
    buf[n] = 0;

    int plen = str_len(pattern);
    int found = 0;

    for (int i = 0; i <= n - plen; i++) {
        int match = 1;
        for (int j = 0; j < plen; j++) {
            if (buf[i + j] != pattern[j]) { match = 0; break; }
        }
        if (match) {
            int start = i;
            while (start > 0 && buf[start - 1] != '\n') start--;
            int end = i;
            while (end < n && buf[end] != '\n') end++;

            for (int k = start; k < end; k++) screen_putchar(buf[k]);
            screen_write("\n");
            found = 1;
            i = end;
        }
    }

    if (!found) kprintf("(sin coincidencias en %s)\n", filename);
}

static void cmd_head(const char *args) {
    char filename[64], numstr[16];
    if (parse_args(args, filename, numstr, 64, 16) < 2) {
        screen_write("Uso: head <archivo> <n>\n");
        return;
    }

    int n_lines = 0;
    for (int i = 0; numstr[i] >= '0' && numstr[i] <= '9'; i++) {
        n_lines = n_lines * 10 + (numstr[i] - '0');
    }
    if (n_lines <= 0) n_lines = 10;

    int fd = fs_open(filename);
    if (fd < 0) { kprintf("Error: %d\n", fd); return; }

    int size = fs_size(fd);
    if (size <= 0) { fs_close(fd); return; }

    static char buf[FS_MAX_SIZE + 1];
    int n = fs_read(fd, buf, FS_MAX_SIZE);
    fs_close(fd);
    buf[n] = 0;

    int printed = 0;
    for (int i = 0; i < n && printed < n_lines; i++) {
        screen_putchar(buf[i]);
        if (buf[i] == '\n') printed++;
    }
}

static void cmd_tail(const char *args) {
    char filename[64], numstr[16];
    if (parse_args(args, filename, numstr, 64, 16) < 2) {
        screen_write("Uso: tail <archivo> <n>\n");
        return;
    }

    int n_lines = 0;
    for (int i = 0; numstr[i] >= '0' && numstr[i] <= '9'; i++) {
        n_lines = n_lines * 10 + (numstr[i] - '0');
    }
    if (n_lines <= 0) n_lines = 10;

    int fd = fs_open(filename);
    if (fd < 0) { kprintf("Error: %d\n", fd); return; }

    int size = fs_size(fd);
    if (size <= 0) { fs_close(fd); return; }

    static char buf[FS_MAX_SIZE + 1];
    int n = fs_read(fd, buf, FS_MAX_SIZE);
    fs_close(fd);
    buf[n] = 0;

    int total_lines = 0;
    for (int i = 0; i < n; i++) if (buf[i] == '\n') total_lines++;

    int skip = total_lines - n_lines;
    if (skip < 0) skip = 0;

    int current = 0;
    int started = 0;
    for (int i = 0; i < n; i++) {
        if (!started && current >= skip) started = 1;
        if (started) screen_putchar(buf[i]);
        if (buf[i] == '\n') current++;
    }
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

    if (args[0] < '0' || args[0] > '9') {
        screen_write("Error: <lba> debe ser un numero\n");
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
        screen_write("  ");
        const char *hex = "0123456789ABCDEF";
        unsigned int off = row * 16;
        screen_putchar(hex[(off >> 12) & 0xF]);
        screen_putchar(hex[(off >> 8) & 0xF]);
        screen_putchar(hex[(off >> 4) & 0xF]);
        screen_putchar(hex[off & 0xF]);
        screen_write(": ");

        for (int col = 0; col < 16; col++) {
            unsigned char b = buf[row * 16 + col];
            screen_putchar(hex[(b >> 4) & 0xF]);
            screen_putchar(hex[b & 0xF]);
            screen_putchar(' ');
        }

        screen_write(" | ");

        for (int col = 0; col < 16; col++) {
            unsigned char b = buf[row * 16 + col];
            if (b >= 32 && b < 127) screen_putchar((char)b);
            else                    screen_putchar('.');
        }
        screen_write("\n");
    }
}

static void cmd_diskwrite(const char *args) {
    if (!ahci_is_ready()) {
        screen_write("AHCI no disponible\n");
        return;
    }

    if (args[0] == 0) {
        screen_write("Uso: diskwrite <lba> <texto>\n");
        return;
    }

    if (args[0] < '0' || args[0] > '9') {
        screen_write("Error: <lba> debe ser un numero\n");
        screen_write("Uso: diskwrite <lba> <texto>\n");
        return;
    }

    uint32_t lba = 0;
    int i = 0;
    while (args[i] >= '0' && args[i] <= '9') {
        lba = lba * 10 + (args[i] - '0');
        i++;
    }
    if (args[i] == ' ') i++;

    const char *text = args + i;

    static unsigned char buf[512];
    for (int k = 0; k < 512; k++) buf[k] = 0;

    int len = 0;
    while (text[len] && len < 511) {
        buf[len] = text[len];
        len++;
    }
    buf[len] = 0;

    if (!ahci_write_sector((uint64_t)lba, buf)) {
        kprintf("Error escribiendo sector %u\n", lba);
        return;
    }

    kprintf("Escritos %d bytes en sector %u\n", len, lba);
}

// =====================================================================
// THREADS
// =====================================================================
static void cmd_ps() {
    thread_list();
}

static void cmd_kill(const char *args) {
    if (args[0] == 0) {
        screen_write("Uso: kill <pid>\n");
        return;
    }
    int pid = 0;
    for (int i = 0; args[i] >= '0' && args[i] <= '9'; i++) {
        pid = pid * 10 + (args[i] - '0');
    }
    if (pid <= 0) {
        screen_write("PID invalido\n");
        return;
    }
    int r = thread_kill(pid);
    if (r < 0) kprintf("No se pudo matar PID %d\n", pid);
    else       kprintf("PID %d terminado\n", pid);
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
    else if (str_eq(l, "free"))         cmd_free();
    else if (str_eq(l, "test"))         cmd_test();
    else if (str_eq(l, "tssinfo"))      cmd_tssinfo();
    else if (str_eq(l, "ring3"))        cmd_ring3();
    else if (str_eq(l, "ls"))           cmd_ls();
    else if (str_eq(l, "df"))           cmd_df();
    else if (str_eq(l, "diskinfo"))     cmd_diskinfo();
    else if (str_eq(l, "ps"))           cmd_ps();
    else if (str_eq(l, "threads"))      cmd_ps();
    else if (str_eq(l, "reboot"))       cmd_reboot();
    else if (str_eq(l, "shutdown"))     cmd_shutdown();
    else if (str_starts_with(l, "echo "))      cmd_echo(l + 5);
    else if (str_starts_with(l, "touch "))     cmd_touch(l + 6);
    else if (str_starts_with(l, "write "))     cmd_write(l + 6);
    else if (str_starts_with(l, "append "))    cmd_append(l + 7);
    else if (str_starts_with(l, "read "))      cmd_read(l + 5);
    else if (str_starts_with(l, "cat "))       cmd_cat(l + 4);
    else if (str_starts_with(l, "cp "))        cmd_cp(l + 3);
    else if (str_starts_with(l, "mv "))        cmd_mv(l + 3);
    else if (str_starts_with(l, "rm "))        cmd_rm(l + 3);
    else if (str_starts_with(l, "wc "))        cmd_wc(l + 3);
    else if (str_starts_with(l, "stat "))      cmd_stat(l + 5);
    else if (str_starts_with(l, "find "))      cmd_find(l + 5);
    else if (str_starts_with(l, "grep "))      cmd_grep(l + 5);
    else if (str_starts_with(l, "head "))      cmd_head(l + 5);
    else if (str_starts_with(l, "tail "))      cmd_tail(l + 5);
    else if (str_starts_with(l, "diskread "))  cmd_diskread(l + 9);
    else if (str_starts_with(l, "diskwrite ")) cmd_diskwrite(l + 10);
    else if (str_starts_with(l, "kill "))      cmd_kill(l + 5);
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
        if (!c) {
            thread_yield();     // ceder CPU mientras no hay tecla
            continue;
        }

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
