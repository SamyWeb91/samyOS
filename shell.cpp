// shell.cpp
#include "shell.h"
#include "win.h"
#include "screen.h"
#include "rtc.h"
#include "pit.h"

/* =====================================================================
 * Linea de entrada por terminal
 * ===================================================================== */
#define TERM_LINE_MAX 64

typedef struct {
    int  wid;                       /* -1 si libre */
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

/* =====================================================================
 * Comandos
 * ===================================================================== */
static void cmd_help(int wid) {
    term_println(wid, "Comandos graficos:");
    term_println(wid, "  help     - esta ayuda");
    term_println(wid, "  clear    - limpiar terminal");
    term_println(wid, "  date     - fecha y hora");
    term_println(wid, "  ticks    - ticks del sistema");
    term_println(wid, "  uptime   - tiempo desde arranque");
    term_println(wid, "  echo ... - imprimir texto");
    term_println(wid, "  close    - cerrar esta ventana");
    term_println(wid, "  reboot   - reiniciar");
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
    if (t == 0) {
        tmp[n++] = '0';
    } else {
        while (t) {
            tmp[n++] = '0' + (t % 10);
            t /= 10;
        }
    }
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

static void cmd_execute(int wid, ShellTerm *t, const char *cmd) {
    if (cmd[0] == 0) return;

    if (str_eq(cmd, "help"))         cmd_help(wid);
    else if (str_eq(cmd, "clear"))   win_sys_clear(wid);
    else if (str_eq(cmd, "date"))    cmd_date(wid);
    else if (str_eq(cmd, "ticks"))   cmd_ticks(wid);
    else if (str_eq(cmd, "uptime"))  cmd_uptime(wid);
    else if (str_starts_with(cmd, "echo ")) term_println(wid, cmd + 5);
    else if (str_eq(cmd, "close")) {
        /* La terminal pide cerrarse. Lo hace el shell al volver. */
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
    }
    shell_ready = 1;
    return 0;
}

extern "C" int shell_new_terminal(int x, int y, int w, int h, const char *title) {
    if (!shell_ready) return -1;

    /* Buscar slot libre */
    int slot = -1;
    for (int i = 0; i < SHELL_MAX_TERMS; i++) {
        if (terms[i].wid < 0) { slot = i; break; }
    }
    if (slot < 0) return -1;

    int wid = win_sys_create(x, y, w, h, title);
    if (wid < 0) return -1;

    terms[slot].wid = wid;
    terms[slot].len = 0;
    terms[slot].line[0] = 0;

    term_println(wid, "MiniSO - Terminal grafica");
    term_println(wid, "Escribe 'help' para ver comandos");
    term_println(wid, "");
    term_prompt(wid);

    return wid;
}

extern "C" void shell_handle_key(int wid, char c) {
    if (wid < 0) return;

    /* Buscar el slot */
    ShellTerm *t = 0;
    for (int i = 0; i < SHELL_MAX_TERMS; i++) {
        if (terms[i].wid == wid) { t = &terms[i]; break; }
    }
    if (!t) return;

    if (c == '\n') {
        win_sys_write(wid, "\n", 1);
        t->line[t->len] = 0;
        cmd_execute(wid, t, t->line);
        t->len = 0;
        /* Si el comando cerro la terminal, no hacer mas */
        if (t->wid < 0) return;
        term_prompt(wid);
    }
    else if (c == '\b') {
        if (t->len > 0) {
            t->len--;
            win_sys_write(wid, "\b", 1);
        }
    }
    else if (c >= 32 && c < 127) {
        if (t->len < TERM_LINE_MAX - 1) {
            t->line[t->len++] = c;
            char ch[2] = { c, 0 };
            win_sys_write(wid, ch, 1);
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

extern "C" int shell_is_terminal(int wid) {
    if (wid < 0) return 0;
    for (int i = 0; i < SHELL_MAX_TERMS; i++) {
        if (terms[i].wid == wid) return 1;
    }
    return 0;
}
