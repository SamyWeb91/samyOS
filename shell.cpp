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

// shell.cpp
#include "shell.h"
#include "win.h"
#include "screen.h"
#include "rtc.h"
#include "pit.h"
#include "composer.h"
#include "ahci.h"
#include "disk.h"
#include "fat32.h"

/* Prueba aislada de ring 3 (ring3_test.cpp) */
extern "C" void ring3_test_run(void);

/* =====================================================================
 * Forward declarations
 * ===================================================================== */
void term_print(int wid, const char *s);
void term_println(int wid, const char *s);
static void term_prompt(int wid);
void print_uint(int wid, unsigned int v);
static int  str_eq(const char *a, const char *b);
static int  str_starts_with(const char *str, const char *prefix);
static unsigned int str_len(const char *s);

/* =====================================================================
 * Linea de entrada por terminal
 * ===================================================================== */
#define TERM_LINE_MAX 64

typedef struct {
    int  wid;
    char line[TERM_LINE_MAX];
    int  len;
} ShellTerm;

static ShellTerm terms[SHELL_MAX_TERMS];
static int shell_ready = 0;

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

static int str_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++; prefix++;
    }
    return 1;
}

static unsigned int str_len(const char *s) {
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

/* =====================================================================
 * Helpers de impresion
 * ===================================================================== */
void term_print(int wid, const char *s) {
    if (wid < 0) return;
    int n = 0;
    while (s[n]) n++;
    win_sys_write(wid, s, n);
}

void term_println(int wid, const char *s) {
    term_print(wid, s);
    term_print(wid, "\n");
}

static void term_prompt(int wid) {
    term_print(wid, "> ");
}

void print_uint(int wid, unsigned int v) {
    char buf[16];
    int n = 0;
    if (v == 0) buf[n++] = '0';
    while (v) { buf[n++] = '0' + (v % 10); v /= 10; }
    char out[16];
    int k = 0;
    for (int i = n - 1; i >= 0; i--) out[k++] = buf[i];
    out[k] = 0;
    term_print(wid, out);
}

/* =====================================================================
 * Callback para listar FAT32
 * ===================================================================== */
static void fat32_shell_list_cb(const char *name, uint32_t size, uint8_t attr, uint32_t cluster) {
    (void)cluster;
    int wid = win_get_focused();

    term_print(wid, "  ");
    term_print(wid, name);
    for (unsigned int k = str_len(name); k < 20; k++) term_print(wid, " ");
    if (attr & FAT32_ATTR_DIRECTORY) {
        term_println(wid, "<DIR>");
    } else {
        print_uint(wid, size);
        term_println(wid, " bytes");
    }
}

/* =====================================================================
 * Comandos
 * ===================================================================== */
static void cmd_help(int wid) {
    term_println(wid, "Comandos:");
    term_println(wid, "  help       - esta ayuda");
    term_println(wid, "  clear      - limpiar terminal");
    term_println(wid, "  date       - fecha y hora");
    term_println(wid, "  ls         - listar archivos (FAT32)");
    term_println(wid, "  cat <n>    - mostrar archivo (FAT32)");
    term_println(wid, "  info       - info de FAT32 montada");
    term_println(wid, "  disk       - estado del disco AHCI");
    term_println(wid, "  partitions - lista particiones");
    term_println(wid, "  hex <lba>  - volcar sector");
    term_println(wid, "  ring3      - cargar TEST.BIN del disco y ejecutar en Ring 3");
    term_println(wid, "  close      - cerrar esta ventana");
}

static void cmd_date(int wid) {
    RTCTime t; rtc_read(&t);
    char buf[20];
    buf[0] = '0' + (t.day/10); buf[1] = '0' + (t.day%10); buf[2] = '/';
    buf[3] = '0' + (t.month/10); buf[4] = '0' + (t.month%10); buf[5] = '/';
    buf[6] = '0' + ((t.year/1000)%10); buf[7] = '0' + ((t.year/100)%10);
    buf[8] = '0' + ((t.year/10)%10); buf[9] = '0' + (t.year%10);
    buf[10] = ' '; buf[11] = '0' + (t.hour/10); buf[12] = '0' + (t.hour%10);
    buf[13] = ':'; buf[14] = '0' + (t.minute/10); buf[15] = '0' + (t.minute%10);
    buf[16] = 0;
    term_println(wid, buf);
}

/* Comentado porque folder ya no se usa
static void cmd_files(int wid) {
    if (folder_open(120, 100, 400, 320) < 0)
        term_println(wid, "files: no se pudo abrir");
    else
        term_println(wid, "files: abriendo gestor");
}
*/

static void cmd_fatls(int wid, const char *path) {
    if (!fat32_is_mounted()) {
        term_println(wid, "ls: FAT32 no montada");
        return;
    }
    if (!path || !*path) path = "/";
    if (!fat32_list(path, fat32_shell_list_cb))
        term_println(wid, "ls: error al listar");
}

static void cmd_fatcat(int wid, const char *name) {
    if (!fat32_is_mounted()) {
        term_println(wid, "cat: FAT32 no montada");
        return;
    }
    if (!name || !*name) {
        term_println(wid, "uso: cat <archivo>");
        return;
    }

    static char buf[512]; // Estático para no inflar el stack
    int r = fat32_read_file(name, buf, sizeof(buf) - 1);
    if (r < 0) {
        term_println(wid, "cat: error al leer archivo");
        return;
    }
    buf[r] = 0;
    term_println(wid, buf);
}

static void cmd_fatinfo(int wid) {
    if (!fat32_is_mounted()) {
        term_println(wid, "info: FAT32 no montada");
        return;
    }
    const Fat32Volume *v = fat32_get_volume();
    if (!v) return;

    term_println(wid, "=== FAT32 Info ===");
    term_print(wid, "Etiqueta: "); term_println(wid, v->volume_label);
    term_print(wid, "LBA: "); print_uint(wid, v->part_lba); term_println(wid, "");
}

/* =====================================================================
 * Dispatcher de comandos
 * ===================================================================== */
static void cmd_execute(int wid, ShellTerm *t, const char *cmd) {
    if (cmd[0] == 0) return;

    /* Comandos del disco */
    if      (str_eq(cmd, "disk"))               disk_cmd_disk(wid);
    else if (str_eq(cmd, "partitions"))         disk_cmd_partitions(wid);
    else if (str_starts_with(cmd, "hex "))      disk_cmd_hex(wid, cmd + 4);

    /* Comandos de FAT32 */
    else if (str_eq(cmd, "ls"))                 cmd_fatls(wid, "/");
    else if (str_starts_with(cmd, "ls "))       cmd_fatls(wid, cmd + 3);
    else if (str_starts_with(cmd, "cat "))      cmd_fatcat(wid, cmd + 4);
    else if (str_eq(cmd, "info"))               cmd_fatinfo(wid);

    /* Comandos basicos */
    else if (str_eq(cmd, "help"))         cmd_help(wid);
    else if (str_eq(cmd, "clear"))        win_sys_clear(wid);
    else if (str_eq(cmd, "date"))         cmd_date(wid);
    // else if (str_eq(cmd, "files"))        cmd_files(wid); // Comentado

    /* Prueba de ring 3 */
    else if (str_eq(cmd, "ring3"))        ring3_test_run();

    else if (str_eq(cmd, "close")) {
        composer_dirty_window(wid);
        t->wid = -1;
        win_sys_destroy(wid);
        return;
    }
    else {
        term_print(wid, "Desconocido: ");
        term_println(wid, cmd);
    }
}

/* =====================================================================
 * API publica
 * ===================================================================== */
extern "C" int shell_init(void) {
    for (int i = 0; i < SHELL_MAX_TERMS; i++) {
        terms[i].wid = -1;
        terms[i].len = 0;
        terms[i].line[0] = 0;
    }
    shell_ready = 1;
    disk_init();
    return 0;
}

extern "C" int shell_new_terminal(int x, int y, int w, int h, const char *title) {
    if (!shell_ready) return -1;

    int slot = -1;
    for (int i = 0; i < SHELL_MAX_TERMS; i++) {
        if (terms[i].wid < 0) { slot = i; break; }
    }
    if (slot < 0) return -1;

    int wid = win_sys_create(x, y, w, h, title);
    if (wid < 0) return -1;

    win_focus(wid);
    composer_dirty_window(wid);

    terms[slot].wid = wid;
    terms[slot].len = 0;
    terms[slot].line[0] = 0;

    term_println(wid, "MiniSO - Terminal grafica");
    term_println(wid, "Escribe 'help' para ver comandos");
    term_println(wid, "");
    term_prompt(wid);

    composer_dirty_window(wid);
    return wid;
}

extern "C" void shell_handle_key(int wid, char c) {
    if (wid < 0) return;

    /* Comentado porque folder ya no se usa
    if (folder_is_folder(wid)) {
        folder_handle_key(wid, c);
        return;
    }
    */

    ShellTerm *t = 0;
    for (int i = 0; i < SHELL_MAX_TERMS; i++) {
        if (terms[i].wid == wid) { t = &terms[i]; break; }
    }
    if (!t) return;

    if (c == 0x49) {  /* Page Up */
        win_sys_scroll_up(wid);
        composer_dirty_window(wid);
        return;
    }
    if (c == 0x51) {  /* Page Down */
        win_sys_scroll_down(wid);
        composer_dirty_window(wid);
        return;
    }

    if (c == '\n' || c == '\b' || (c >= 32 && c < 127)) {
        win_sys_scroll_reset(wid);
    }

    if (c == '\n') {
        win_sys_write(wid, "\n", 1);
        t->line[t->len] = 0;
        cmd_execute(wid, t, t->line);
        t->len = 0;
        if (t->wid < 0) return;
        term_prompt(wid);
        composer_dirty_window(wid);
    }
    else if (c == '\b') {
        if (t->len > 0) {
            t->len--;
            win_sys_write(wid, "\b", 1);
            composer_dirty_window(wid);
        }
    }
    else if (c >= 32 && c < 127) {
        if (t->len < TERM_LINE_MAX - 1) {
            t->line[t->len++] = c;
            char ch[2] = { c, 0 };
            win_sys_write(wid, ch, 1);
            composer_dirty_window(wid);
        }
    }
}

extern "C" void shell_close_terminal(int wid) {
    for (int i = 0; i < SHELL_MAX_TERMS; i++) {
        if (terms[i].wid == wid) {
            terms[i].wid = -1;
            terms[i].len = 0;
            return;
        }
    }
}

/* Wrappers para disk.cpp */
extern "C" void shell_term_print(int wid, const char *s) { term_print(wid, s); }
extern "C" void shell_term_println(int wid, const char *s) { term_println(wid, s); }
extern "C" void shell_print_uint(int wid, unsigned int v) { print_uint(wid, v); }

extern "C" int shell_is_terminal(int wid) {
    if (wid < 0) return 0;
    for (int i = 0; i < SHELL_MAX_TERMS; i++) {
        if (terms[i].wid == wid) return 1;
    }
    return 0;
}
