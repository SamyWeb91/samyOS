// shell.cpp
#include "shell.h"
#include "win.h"
#include "screen.h"
#include "rtc.h"
#include "pit.h"
#include "ramdisk.h"
#include "libc.h"
#include "composer.h"
#include "mfs.h"
#include "folder.h"
#include "ahci.h"
#include "disk.h"

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
#define EDIT_BUF_MAX  3800

typedef struct {
    int  wid;
    char line[TERM_LINE_MAX];
    int  len;

    int  editing;
    char edit_name[16];
    int  edit_len;
    char edit_buf[EDIT_BUF_MAX];
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
 * Editor grafico
 * ===================================================================== */
static void editor_draw(int wid, ShellTerm *t) {
    win_sys_clear(wid);

    term_print(wid, "=== EDITOR: ");
    term_print(wid, t->edit_name);
    term_println(wid, " ===");
    term_println(wid, "Ctrl+S=guardar  ESC=salir");
    term_println(wid, "-----------------------------");

    if (t->edit_len == 0) {
        term_println(wid, "(vacio)");
    } else {
        win_sys_write(wid, t->edit_buf, t->edit_len);
    }

    composer_dirty_window(wid);
}

static int editor_open(int x, int y, int w, int h, const char *name) {
    int slot = -1;
    for (int i = 0; i < SHELL_MAX_TERMS; i++) {
        if (terms[i].wid < 0) { slot = i; break; }
    }
    if (slot < 0) return -1;

    char title[32];
    title[0] = 'E'; title[1] = 'd'; title[2] = 'i'; title[3] = 't';
    title[4] = ':'; title[5] = ' ';
    int k = 6;
    int n = 0;
    while (name[n] && k < 30) { title[k++] = name[n++]; }
    title[k] = 0;

    int wid = win_sys_create(x, y, w, h, title);
    if (wid < 0) return -1;

    win_focus(wid);

    ShellTerm *t = &terms[slot];
    t->wid = wid;
    t->len = 0;
    t->line[0] = 0;
    t->editing = 1;

    k = 0;
    while (name[k] && k < 15) { t->edit_name[k] = name[k]; k++; }
    t->edit_name[k] = 0;

    int r = mfs_read_file(t->edit_name, t->edit_buf, EDIT_BUF_MAX - 1);
    if (r < 0) t->edit_len = 0;
    else       t->edit_len = r;

    editor_draw(wid, t);
    composer_dirty_window(wid);
    return wid;
}

static void editor_handle_key(int wid, ShellTerm *t, char c) {
    if (c == 27) {   /* ESC */
        composer_dirty_window(wid);
        t->wid = -1;
        t->editing = 0;
        win_sys_destroy(wid);
        return;
    }

    if (c == 19) {   /* Ctrl+S */
        int r = mfs_write_file(t->edit_name, t->edit_buf, t->edit_len);
        if (r < 0) {
            editor_draw(wid, t);
            term_println(wid, "\nERROR al guardar");
        } else {
            editor_draw(wid, t);
            term_println(wid, "\nGuardado.");
        }
        return;
    }

    if (c == '\b') {
        if (t->edit_len > 0) {
            t->edit_len--;
            editor_draw(wid, t);
        }
        return;
    }

    if (c == '\n') {
        if (t->edit_len < EDIT_BUF_MAX - 1) {
            t->edit_buf[t->edit_len++] = '\n';
            editor_draw(wid, t);
        }
        return;
    }

    if (c >= 32 && c < 127) {
        if (t->edit_len < EDIT_BUF_MAX - 1) {
            t->edit_buf[t->edit_len++] = c;
            editor_draw(wid, t);
        }
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
    term_println(wid, "  ticks      - ticks del sistema");
    term_println(wid, "  uptime     - tiempo desde arranque");
    term_println(wid, "  echo ...   - imprimir texto");
    term_println(wid, "  ls         - listar archivos");
    term_println(wid, "  touch <n>  - crear archivo vacio");
    term_println(wid, "  cat <n>    - mostrar contenido");
    term_println(wid, "  write <n> <texto> - escribir en archivo");
    term_println(wid, "  rm <n>     - borrar archivo");
    term_println(wid, "  df         - uso del ramdisk");
    term_println(wid, "  edit <n>   - editor grafico");
    term_println(wid, "  files      - gestor grafico de archivos");
    term_println(wid, "  disk       - estado del disco AHCI");
    term_println(wid, "  partitions - lista particiones del MBR");
    term_println(wid, "  hex <lba>  - volcar 512 bytes de un sector");
    term_println(wid, "  close      - cerrar esta ventana");
    term_println(wid, "  reboot     - reiniciar");
    term_println(wid, "");
    term_println(wid, "Scroll: PageUp / PageDown");
}

static void cmd_date(int wid) {
    RTCTime t; rtc_read(&t);
    char buf[48];
    buf[0] = '0' + (t.day/10); buf[1] = '0' + (t.day%10); buf[2] = '/';
    buf[3] = '0' + (t.month/10); buf[4] = '0' + (t.month%10); buf[5] = '/';
    buf[6] = '0' + ((t.year/1000)%10); buf[7] = '0' + ((t.year/100)%10);
    buf[8] = '0' + ((t.year/10)%10); buf[9] = '0' + (t.year%10);
    buf[10] = ' '; buf[11] = '0' + (t.hour/10); buf[12] = '0' + (t.hour%10);
    buf[13] = ':'; buf[14] = '0' + (t.minute/10); buf[15] = '0' + (t.minute%10);
    buf[16] = ':'; buf[17] = '0' + (t.second/10); buf[18] = '0' + (t.second%10);
    buf[19] = 0;
    term_println(wid, buf);
}

static void cmd_ticks(int wid) {
    unsigned int t = pit_get_ticks();
    char tmp[12];
    int n = 0;
    if (t == 0) tmp[n++] = '0';
    else { while (t) { tmp[n++] = '0' + (t % 10); t /= 10; } }
    char buf[24];
    buf[0] = 'T'; buf[1] = 'i'; buf[2] = 'c'; buf[3] = 'k';
    buf[4] = 's'; buf[5] = ':'; buf[6] = ' ';
    for (int k = 0; k < n; k++) buf[7 + k] = tmp[n - 1 - k];
    buf[7 + n] = 0;
    term_println(wid, buf);
}

static void cmd_uptime(int wid) {
    unsigned int t = pit_get_ticks() / 100;
    unsigned int h = t / 3600;
    unsigned int m = (t % 3600) / 60;
    unsigned int s = t % 60;
    char buf[20];
    buf[0] = '0' + (h / 10); buf[1] = '0' + (h % 10); buf[2] = ':';
    buf[3] = '0' + (m / 10); buf[4] = '0' + (m % 10); buf[5] = ':';
    buf[6] = '0' + (s / 10); buf[7] = '0' + (s % 10); buf[8] = 0;
    term_print(wid, "Uptime: ");
    term_println(wid, buf);
}

static void cmd_ls(int wid) {
    if (!mfs_mounted()) { term_println(wid, "fs: no montado"); return; }

    int n = mfs_count();
    if (n == 0) { term_println(wid, "  (vacio)"); return; }

    for (int i = 0; i < n; i++) {
        char name[16];
        unsigned int size = 0;
        if (mfs_get(i, name, &size) != 0) continue;

        term_print(wid, "  ");
        term_print(wid, name);
        for (unsigned int k = str_len(name); k < 12; k++) term_print(wid, " ");
        print_uint(wid, size);
        term_println(wid, " bytes");
    }
}

static void cmd_touch(int wid, const char *name) {
    if (!mfs_mounted()) { term_println(wid, "fs: no montado"); return; }
    if (!name || !*name) { term_println(wid, "uso: touch <nombre>"); return; }

    int r = mfs_create(name);
    if (r == -2)      term_println(wid, "touch: ya existe");
    else if (r < 0)   term_println(wid, "touch: error");
    else              term_println(wid, "touch: ok");
}

static void cmd_cat(int wid, const char *name) {
    if (!mfs_mounted()) { term_println(wid, "fs: no montado"); return; }
    if (!name || !*name) { term_println(wid, "uso: cat <nombre>"); return; }

    char buf[512];
    int r = mfs_read_file(name, buf, sizeof(buf) - 1);
    if (r < 0) { term_println(wid, "cat: no existe"); return; }
    buf[r] = 0;
    term_println(wid, buf);
}

static void cmd_write(int wid, const char *rest) {
    if (!mfs_mounted()) { term_println(wid, "fs: no montado"); return; }

    char name[16];
    int i = 0;
    while (rest[i] && rest[i] != ' ' && i < 15) { name[i] = rest[i]; i++; }
    name[i] = 0;

    if (i == 0) { term_println(wid, "uso: write <nombre> <texto>"); return; }

    while (rest[i] == ' ') i++;
    const char *text = rest + i;

    int r = mfs_write_file(name, text, str_len(text));
    if (r < 0) term_println(wid, "write: error");
    else       term_println(wid, "write: ok");
}

static void cmd_rm(int wid, const char *name) {
    if (!mfs_mounted()) { term_println(wid, "fs: no montado"); return; }
    if (!name || !*name) { term_println(wid, "uso: rm <nombre>"); return; }

    int r = mfs_delete(name);
    if (r < 0) term_println(wid, "rm: no existe");
    else       term_println(wid, "rm: ok");
}

static void cmd_df(int wid) {
    if (!mfs_mounted()) { term_println(wid, "fs: no montado"); return; }

    unsigned int total = ramdisk_size(MFS_RD_ID);
    unsigned int used = 0;
    int n = mfs_count();
    for (int i = 0; i < n; i++) {
        char name[16];
        unsigned int size = 0;
        if (mfs_get(i, name, &size) == 0) used += size;
    }

    term_print(wid, "Total:  "); print_uint(wid, total);     term_println(wid, " bytes");
    term_print(wid, "Datos:  "); print_uint(wid, used);      term_println(wid, " bytes usados");
    term_print(wid, "Libre:  "); print_uint(wid, total - used); term_println(wid, " bytes");
    term_print(wid, "Archivos: "); print_uint(wid, (unsigned int)n); term_println(wid, "");
}

static void cmd_edit(int shell_wid, const char *name) {
    if (!mfs_mounted()) { term_println(shell_wid, "fs: no montado"); return; }
    if (!name || !*name) { term_println(shell_wid, "uso: edit <nombre>"); return; }

    if (!mfs_exists(name)) {
        if (mfs_create(name) != 0) {
            term_println(shell_wid, "edit: no se pudo crear");
            return;
        }
    }

    int wid = editor_open(80, 80, 460, 340, name);
    if (wid < 0) {
        term_println(shell_wid, "edit: no hay slots libres");
        return;
    }

    term_print(shell_wid, "edit: abriendo ");
    term_println(shell_wid, name);
}

static void cmd_files(int wid) {
    if (folder_open(120, 100, 400, 320) < 0) {
        term_println(wid, "files: no se pudo abrir");
    } else {
        term_println(wid, "files: abriendo gestor");
    }
}

/* =====================================================================
 * Dispatcher de comandos
 * ===================================================================== */
static void cmd_execute(int wid, ShellTerm *t, const char *cmd) {
    if (cmd[0] == 0) return;

    /* Comandos del disco (disk.cpp) */
    if      (str_eq(cmd, "disk"))               disk_cmd_disk(wid);
    else if (str_eq(cmd, "partitions"))         disk_cmd_partitions(wid);
    else if (str_starts_with(cmd, "hex "))      disk_cmd_hex(wid, cmd + 4);

    /* Resto de comandos del shell */
    else if (str_eq(cmd, "help"))         cmd_help(wid);
    else if (str_eq(cmd, "clear"))        win_sys_clear(wid);
    else if (str_eq(cmd, "date"))         cmd_date(wid);
    else if (str_eq(cmd, "ticks"))        cmd_ticks(wid);
    else if (str_eq(cmd, "uptime"))       cmd_uptime(wid);
    else if (str_eq(cmd, "ls"))           cmd_ls(wid);
    else if (str_eq(cmd, "df"))           cmd_df(wid);
    else if (str_eq(cmd, "files"))        cmd_files(wid);
    else if (str_starts_with(cmd, "echo "))  term_println(wid, cmd + 5);
    else if (str_starts_with(cmd, "touch ")) cmd_touch(wid, cmd + 6);
    else if (str_starts_with(cmd, "cat "))   cmd_cat(wid, cmd + 4);
    else if (str_starts_with(cmd, "write ")) cmd_write(wid, cmd + 6);
    else if (str_starts_with(cmd, "rm "))    cmd_rm(wid, cmd + 3);
    else if (str_starts_with(cmd, "edit "))  cmd_edit(wid, cmd + 5);
    else if (str_eq(cmd, "close")) {
        composer_dirty_window(wid);
        t->wid = -1;
        win_sys_destroy(wid);
        return;
    }
    else if (str_eq(cmd, "reboot")) {
        term_println(wid, "Reiniciando...");
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
        terms[i].editing = 0;
        terms[i].edit_len = 0;
        terms[i].edit_name[0] = 0;
    }
    shell_ready = 1;

    disk_init();
    mfs_mount();
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
    terms[slot].editing = 0;
    terms[slot].edit_len = 0;
    terms[slot].edit_name[0] = 0;

    term_println(wid, "MiniSO - Terminal grafica");
    term_println(wid, "Escribe 'help' para ver comandos");
    term_println(wid, "");
    term_prompt(wid);

    composer_dirty_window(wid);
    return wid;
}

extern "C" void shell_handle_key(int wid, char c) {
    if (wid < 0) return;

    if (folder_is_folder(wid)) {
        folder_handle_key(wid, c);
        return;
    }

    ShellTerm *t = 0;
    for (int i = 0; i < SHELL_MAX_TERMS; i++) {
        if (terms[i].wid == wid) { t = &terms[i]; break; }
    }
    if (!t) return;

    if (t->editing) {
        editor_handle_key(wid, t, c);
        return;
    }

    /* ================================================================
     * SCROLL: Page Up / Page Down
     * ================================================================
     * Scancodes estandar PS/2:
     *   Page Up   = 0x49
     *   Page Down = 0x51
     * Si tu driver los traduce a otros valores, ajustalos aqui.
     * ================================================================ */
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

    /* Si el usuario escribe algo, volver al fondo del scroll */
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
            terms[i].editing = 0;
            return;
        }
    }
}

/* Wrappers para que disk.cpp y otros modulos usen estas funciones */
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
