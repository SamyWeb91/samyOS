// shell.cpp
#include "shell.h"
#include "win.h"
#include "screen.h"
#include "rtc.h"
#include "pit.h"
#include "ramdisk.h"
#include "libc.h"
#include "composer.h"     /* <-- NUEVO */

/* =====================================================================
 * Forward declarations
 * ===================================================================== */
static void term_print(int wid, const char *s);
static void term_println(int wid, const char *s);
static void term_prompt(int wid);
static void print_uint(int wid, unsigned int v);
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

    /* Editor */
    int  editing;
    char edit_name[16];
    int  edit_len;
    char edit_buf[EDIT_BUF_MAX];
} ShellTerm;

static ShellTerm terms[SHELL_MAX_TERMS];
static int shell_ready = 0;

/* =====================================================================
 * Mini-FS sobre el ramdisk
 * ===================================================================== */
#define FS_RD_ID          0
#define FS_MAGIC          0x5346524Du
#define FS_HEADER_SIZE    0x10
#define FS_MAX_FILES      16
#define FS_ENTRY_SIZE     32
#define FS_TABLE_SIZE     (FS_MAX_FILES * FS_ENTRY_SIZE)
#define FS_DATA_START     (FS_HEADER_SIZE + FS_TABLE_SIZE)

typedef struct {
    unsigned int magic;
    unsigned int file_count;
    unsigned int reserved0;
    unsigned int reserved1;
} FsHeader;

typedef struct {
    unsigned char used;
    char          name[12];
    unsigned char pad[3];
    unsigned int  offset;
    unsigned int  size;
    unsigned char pad2[8];
} FsEntry;

static int fs_mounted = 0;

static void mem_zero(void *p, unsigned int n) {
    unsigned char *b = (unsigned char *)p;
    for (unsigned int i = 0; i < n; i++) b[i] = 0;
}

static int fs_read_header(FsHeader *h) {
    if (ramdisk_read(FS_RD_ID, 0, h, sizeof(FsHeader)) != (int)sizeof(FsHeader))
        return -1;
    return 0;
}

static int fs_write_header(const FsHeader *h) {
    if (ramdisk_write(FS_RD_ID, 0, h, sizeof(FsHeader)) != (int)sizeof(FsHeader))
        return -1;
    return 0;
}

static int fs_read_entry(int i, FsEntry *e) {
    unsigned int off = FS_HEADER_SIZE + i * FS_ENTRY_SIZE;
    if (ramdisk_read(FS_RD_ID, off, e, sizeof(FsEntry)) != (int)sizeof(FsEntry))
        return -1;
    return 0;
}

static int fs_write_entry(int i, const FsEntry *e) {
    unsigned int off = FS_HEADER_SIZE + i * FS_ENTRY_SIZE;
    if (ramdisk_write(FS_RD_ID, off, e, sizeof(FsEntry)) != (int)sizeof(FsEntry))
        return -1;
    return 0;
}

static int fs_mount(void) {
    if (!ramdisk_exists(FS_RD_ID)) return -1;

    FsHeader h;
    fs_read_header(&h);

    if (h.magic != FS_MAGIC) {
        h.magic = FS_MAGIC;
        h.file_count = 0;
        h.reserved0 = 0;
        h.reserved1 = 0;
        if (fs_write_header(&h) != 0) return -1;

        FsEntry zero;
        mem_zero(&zero, sizeof(zero));
        for (int i = 0; i < FS_MAX_FILES; i++) {
            fs_write_entry(i, &zero);
        }
    }

    fs_mounted = 1;
    return 0;
}

static int fs_find(const char *name) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        FsEntry e;
        if (fs_read_entry(i, &e) != 0) return -1;
        if (!e.used) continue;
        if (str_eq(e.name, name)) return i;
    }
    return -1;
}

static int fs_create(const char *name) {
    if (str_len(name) > 11) return -1;
    if (fs_find(name) >= 0) return -2;

    for (int i = 0; i < FS_MAX_FILES; i++) {
        FsEntry e;
        if (fs_read_entry(i, &e) != 0) return -1;
        if (!e.used) {
            unsigned int data_off = 0;
            for (int j = 0; j < FS_MAX_FILES; j++) {
                FsEntry o;
                if (fs_read_entry(j, &o) != 0) return -1;
                if (o.used) {
                    unsigned int end = o.offset + o.size;
                    if (end > data_off) data_off = end;
                }
            }
            if (FS_DATA_START + data_off >= ramdisk_size(FS_RD_ID))
                return -3;

            mem_zero(&e, sizeof(e));
            e.used = 1;
            int k = 0;
            while (name[k] && k < 11) { e.name[k] = name[k]; k++; }
            e.name[k] = 0;
            e.offset = data_off;
            e.size = 0;

            if (fs_write_entry(i, &e) != 0) return -1;

            FsHeader h;
            fs_read_header(&h);
            h.file_count++;
            fs_write_header(&h);
            return 0;
        }
    }
    return -4;
}

static int fs_delete(const char *name) {
    int idx = fs_find(name);
    if (idx < 0) return -1;

    FsEntry e;
    fs_read_entry(idx, &e);
    mem_zero(&e, sizeof(FsEntry));
    fs_write_entry(idx, &e);

    FsHeader h;
    fs_read_header(&h);
    if (h.file_count > 0) h.file_count--;
    fs_write_header(&h);
    return 0;
}

static int fs_write_file(const char *name, const char *data, unsigned int len) {
    int idx = fs_find(name);
    if (idx < 0) return -1;

    FsEntry e;
    fs_read_entry(idx, &e);

    unsigned int max = ramdisk_size(FS_RD_ID) - FS_DATA_START - e.offset;
    if (len > max) len = max;

    if (ramdisk_write(FS_RD_ID, FS_DATA_START + e.offset, data, len) != (int)len)
        return -1;

    e.size = len;
    fs_write_entry(idx, &e);
    return (int)len;
}

static int fs_read_file(const char *name, char *buf, unsigned int max) {
    int idx = fs_find(name);
    if (idx < 0) return -1;

    FsEntry e;
    fs_read_entry(idx, &e);

    unsigned int n = e.size;
    if (n > max) n = max;

    if (ramdisk_read(FS_RD_ID, FS_DATA_START + e.offset, buf, n) != (int)n)
        return -1;
    return (int)n;
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
static void term_print(int wid, const char *s) {
    if (wid < 0) return;
    int n = 0;
    while (s[n]) n++;
    win_sys_write(wid, s, n);
}

static void term_println(int wid, const char *s) {
    term_print(wid, s);
    term_print(wid, "\n");
}

static void term_prompt(int wid) {
    term_print(wid, "> ");
}

static void print_uint(int wid, unsigned int v) {
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

    /* Cabecera */
    term_print(wid, "=== EDITOR: ");
    term_print(wid, t->edit_name);
    term_println(wid, " ===");
    term_println(wid, "Ctrl+S=guardar  ESC=salir");
    term_println(wid, "-----------------------------");

    /* Contenido */
    if (t->edit_len == 0) {
        term_println(wid, "(vacio)");
    } else {
        win_sys_write(wid, t->edit_buf, t->edit_len);
    }

    /* Marcar la ventana como sucia para que el compositor la repinte */
    composer_dirty_window(wid);
}

static int editor_open(int x, int y, int w, int h, const char *name) {
    /* Buscar slot */
    int slot = -1;
    for (int i = 0; i < SHELL_MAX_TERMS; i++) {
        if (terms[i].wid < 0) { slot = i; break; }
    }
    if (slot < 0) return -1;

    /* Titulo "Edit: nombre" */
    char title[32];
    title[0] = 'E'; title[1] = 'd'; title[2] = 'i'; title[3] = 't';
    title[4] = ':'; title[5] = ' ';
    int k = 6;
    int n = 0;
    while (name[n] && k < 30) { title[k++] = name[n++]; }
    title[k] = 0;

    int wid = win_sys_create(x, y, w, h, title);
    if (wid < 0) return -1;

    /* Traer al frente: asi el compositor la pinta encima de la terminal
       y no se "borra" cuando el cursor pasa por encima. */
    win_focus(wid);

    ShellTerm *t = &terms[slot];
    t->wid = wid;
    t->len = 0;
    t->line[0] = 0;
    t->editing = 1;

    /* Copiar nombre */
    k = 0;
    while (name[k] && k < 15) { t->edit_name[k] = name[k]; k++; }
    t->edit_name[k] = 0;

    /* Cargar contenido del ramdisk */
    int r = fs_read_file(t->edit_name, t->edit_buf, EDIT_BUF_MAX - 1);
    if (r < 0) t->edit_len = 0;
    else       t->edit_len = r;

    editor_draw(wid, t);

    /* Marcar toda la zona de la ventana nueva para que se pinte ya */
    composer_dirty_window(wid);

    return wid;
}

static void editor_handle_key(int wid, ShellTerm *t, char c) {
    if (c == 27) {   /* ESC */
        /* Marcar la zona de la ventana ANTES de destruirla para que el
           compositor repinte el fondo en su sitio (si no, queda pegada
           la imagen fantasma en el framebuffer). */
        composer_dirty_window(wid);
        t->wid = -1;
        t->editing = 0;
        win_sys_destroy(wid);
        return;
    }

    if (c == 19) {   /* Ctrl+S */
        int r = fs_write_file(t->edit_name, t->edit_buf, t->edit_len);
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
    term_println(wid, "  close      - cerrar esta ventana");
    term_println(wid, "  reboot     - reiniciar");
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
    if (!fs_mounted) { term_println(wid, "fs: no montado"); return; }

    int count = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        FsEntry e;
        if (fs_read_entry(i, &e) != 0) continue;
        if (!e.used) continue;

        term_print(wid, "  ");
        term_print(wid, e.name);
        for (unsigned int k = str_len(e.name); k < 12; k++) term_print(wid, " ");
        print_uint(wid, e.size);
        term_println(wid, " bytes");
        count++;
    }
    if (count == 0) term_println(wid, "  (vacio)");
}

static void cmd_touch(int wid, const char *name) {
    if (!fs_mounted) { term_println(wid, "fs: no montado"); return; }
    if (!name || !*name) { term_println(wid, "uso: touch <nombre>"); return; }

    int r = fs_create(name);
    if (r == -2)      term_println(wid, "touch: ya existe");
    else if (r < 0)   term_println(wid, "touch: error");
    else              term_println(wid, "touch: ok");
}

static void cmd_cat(int wid, const char *name) {
    if (!fs_mounted) { term_println(wid, "fs: no montado"); return; }
    if (!name || !*name) { term_println(wid, "uso: cat <nombre>"); return; }

    char buf[512];
    int r = fs_read_file(name, buf, sizeof(buf) - 1);
    if (r < 0) { term_println(wid, "cat: no existe"); return; }
    buf[r] = 0;
    term_println(wid, buf);
}

static void cmd_write(int wid, const char *rest) {
    if (!fs_mounted) { term_println(wid, "fs: no montado"); return; }

    char name[16];
    int i = 0;
    while (rest[i] && rest[i] != ' ' && i < 15) { name[i] = rest[i]; i++; }
    name[i] = 0;

    if (i == 0) { term_println(wid, "uso: write <nombre> <texto>"); return; }

    while (rest[i] == ' ') i++;
    const char *text = rest + i;

    int idx = fs_find(name);
    if (idx < 0) {
        if (fs_create(name) != 0) { term_println(wid, "write: no se pudo crear"); return; }
    }

    int r = fs_write_file(name, text, str_len(text));
    if (r < 0) term_println(wid, "write: error");
    else       term_println(wid, "write: ok");
}

static void cmd_rm(int wid, const char *name) {
    if (!fs_mounted) { term_println(wid, "fs: no montado"); return; }
    if (!name || !*name) { term_println(wid, "uso: rm <nombre>"); return; }

    int r = fs_delete(name);
    if (r < 0) term_println(wid, "rm: no existe");
    else       term_println(wid, "rm: ok");
}

static void cmd_df(int wid) {
    if (!fs_mounted) { term_println(wid, "fs: no montado"); return; }

    unsigned int total = ramdisk_size(FS_RD_ID);
    unsigned int used_data = 0;

    for (int i = 0; i < FS_MAX_FILES; i++) {
        FsEntry e;
        if (fs_read_entry(i, &e) != 0) continue;
        if (e.used) used_data += e.size;
    }

    term_print(wid, "Total:  "); print_uint(wid, total);           term_println(wid, " bytes");
    term_print(wid, "Tabla:  "); print_uint(wid, FS_DATA_START);   term_println(wid, " bytes");
    term_print(wid, "Datos:  "); print_uint(wid, used_data);       term_println(wid, " bytes usados");
    term_print(wid, "Libre:  ");
    print_uint(wid, total - FS_DATA_START - used_data);
    term_println(wid, " bytes");
}

static void cmd_edit(int shell_wid, const char *name) {
    if (!fs_mounted) { term_println(shell_wid, "fs: no montado"); return; }
    if (!name || !*name) { term_println(shell_wid, "uso: edit <nombre>"); return; }

    /* Si no existe, crearlo vacio */
    if (fs_find(name) < 0) {
        if (fs_create(name) != 0) {
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

static void cmd_execute(int wid, ShellTerm *t, const char *cmd) {
    if (cmd[0] == 0) return;

    if (str_eq(cmd, "help"))         cmd_help(wid);
    else if (str_eq(cmd, "clear"))   win_sys_clear(wid);
    else if (str_eq(cmd, "date"))    cmd_date(wid);
    else if (str_eq(cmd, "ticks"))   cmd_ticks(wid);
    else if (str_eq(cmd, "uptime"))  cmd_uptime(wid);
    else if (str_eq(cmd, "ls"))      cmd_ls(wid);
    else if (str_eq(cmd, "df"))      cmd_df(wid);
    else if (str_starts_with(cmd, "echo "))  term_println(wid, cmd + 5);
    else if (str_starts_with(cmd, "touch ")) cmd_touch(wid, cmd + 6);
    else if (str_starts_with(cmd, "cat "))   cmd_cat(wid, cmd + 4);
    else if (str_starts_with(cmd, "write ")) cmd_write(wid, cmd + 6);
    else if (str_starts_with(cmd, "rm "))    cmd_rm(wid, cmd + 3);
    else if (str_starts_with(cmd, "edit "))  cmd_edit(wid, cmd + 5);
    else if (str_eq(cmd, "close")) {
        /* Marcar antes de destruir */
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

    if (fs_mount() != 0) {
        /* No fatal */
    }

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

    /* Traer al frente y marcar para repintar */
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

    ShellTerm *t = 0;
    for (int i = 0; i < SHELL_MAX_TERMS; i++) {
        if (terms[i].wid == wid) { t = &terms[i]; break; }
    }
    if (!t) return;

    /* Si esta en modo editor, va al editor */
    if (t->editing) {
        editor_handle_key(wid, t, c);
        return;
    }

    /* Modo shell normal */
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

extern "C" int shell_is_terminal(int wid) {
    if (wid < 0) return 0;
    for (int i = 0; i < SHELL_MAX_TERMS; i++) {
        if (terms[i].wid == wid) return 1;
    }
    return 0;
}
