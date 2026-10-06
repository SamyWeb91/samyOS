// =====================================================================
// edit.cpp
// Editor de texto para ring 3.
//
// Funciones:
//   - Abre un archivo del FS ("hello.txt" hardcodeado)
//   - Muestra el contenido
//   - Permite añadir texto al final
//   - Backspace borra
//   - Ctrl+S guarda
//   - ESC sale
// =====================================================================

#include <stdint.h>
#include <stddef.h>
#include "user_syscalls.h"

// =====================================================================
// Configuración
// =====================================================================
#define BUF_MAX     3800
#define FILENAME    "hello.txt"

// =====================================================================
// Estado del editor
// =====================================================================
static char buf[BUF_MAX];
static int  buf_len = 0;
static int  fd = -1;

// =====================================================================
// Helpers
// =====================================================================
static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void print(const char *s) {
    sys_write(s, str_len(s));
}

static void print_n(const char *s, int n) {
    sys_write(s, n);
}

// =====================================================================
// Cargar archivo
// =====================================================================
static void load_file(void) {
    fd = sys_open(FILENAME);
    if (fd < 0) {
        print("ERROR: no se pudo abrir ");
        print(FILENAME);
        print("\n");
        return;
    }

    int n = sys_read(fd, buf, BUF_MAX - 1);
    if (n > 0) {
        buf_len = n;
    } else {
        buf_len = 0;
    }

    sys_close(fd);
    fd = -1;
}

// =====================================================================
// Guardar archivo
// =====================================================================
static int save_file(void) {
    // Borrar el archivo y recrearlo (truncate)
    sys_close(fd);
    fd = -1;

    // Abrir el archivo en modo escritura (crea si no existe)
    // fs_open crea si no existe, y pos=0
    int wfd = sys_open(FILENAME);
    if (wfd < 0) {
        print("ERROR al abrir para guardar\n");
        return -1;
    }

    // Escribir los bytes del buffer
    int written = sys_write_file(wfd, buf, buf_len);
    sys_close(wfd);

    if (written < 0) {
        print("ERROR al escribir\n");
        return -1;
    }

    return written;
}

// =====================================================================
// Mostrar buffer completo
// =====================================================================
static void redraw(void) {
    sys_clear();
    print("=== EDITOR (ESC=salir, Ctrl+S=guardar) ===\n\n");

    if (buf_len == 0) {
        print("(vacio)\n");
    } else {
        print_n(buf, buf_len);
    }

    print("\n\n--- Escribe. Ctrl+S=guardar ---\n");
}

// =====================================================================
// main del editor
// =====================================================================
extern "C" void edit_main(void) {
    load_file();
    redraw();

    while (1) {
        int key = sys_read_key();
        if (key == 0) continue;      // sin tecla (ya cedió CPU)

        // ESC → salir sin guardar
        if (key == 27) {
            print("\n(salida sin guardar)\n");
            sys_exit(0);
        }

        // Ctrl+S → guardar y salir
        if (key == 19) {
            int n = save_file();
            if (n < 0) {
                print("ERROR al guardar\n");
            } else {
                print("\nGuardado (");
                // print n
                char num[16];
                int p = 0;
                if (n == 0) num[p++] = '0';
                char tmp[16]; int t = 0;
                int nn = n;
                while (nn > 0) { tmp[t++] = '0' + (nn % 10); nn /= 10; }
                while (t > 0) num[p++] = tmp[--t];
                num[p] = 0;
                print(num);
                print(" bytes)\n");
            }
            sys_exit(0);
        }

        // Backspace
        if (key == 8) {
            if (buf_len > 0) {
                buf_len--;
                // Borrar de pantalla: "\b \b"
                sys_putchar(8);
                sys_putchar(' ');
                sys_putchar(8);
            }
            continue;
        }

        // Enter
        if (key == 10) {
            if (buf_len < BUF_MAX - 1) {
                buf[buf_len++] = '\n';
                sys_putchar('\n');
            }
            continue;
        }

        // Caracteres imprimibles
        if (key >= 32 && key < 127) {
            if (buf_len < BUF_MAX - 1) {
                buf[buf_len++] = (char)key;
                sys_putchar((char)key);
            }
        }
    }
}

// =====================================================================
// Punto de entrada (lo busca el loader)
// =====================================================================
extern "C" void user_prog_start(void) {
    edit_main();
    sys_exit(0);
}

extern "C" void user_prog_end(void) {}
