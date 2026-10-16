// desktop.cpp
#include "desktop.h"
#include "gfx.h"
#include "screen.h"
#include "pit.h"
#include "thread.h"
#include "mouse.h"
#include "keyboard.h"
#include "win.h"
#include "cpu_monitor.h"
#include "composer.h"
#include "taskbar_start.h"
#include "shell.h"

// =====================================================================
// >>> INICIO BLOQUE DE TEST FAT32 <<<
// Este bloque es SOLO para probar fat32_win.cpp.
// Cuando fat32 funcione, BORRA este bloque completo
// (desde aqui hasta ">>> FIN BLOQUE DE TEST FAT32 <<<").
// =====================================================================
extern "C" int fat32_mount(uint32_t part_lba);
extern "C" int fat32_is_mounted(void);
extern "C" const void *fat32_get_volume(void);
extern "C" int fat32_list(const char *path,
                          void (*cb)(const char *name, uint32_t size,
                                     uint8_t attr, uint32_t cluster));

#define FAT32_TEST_LBA  2048   /* LBA de la particion FAT32 en disco.img */

/* Callback que se llama por cada entrada del directorio */
static void fat32_test_list_cb(const char *name, uint32_t size,
                               uint8_t attr, uint32_t cluster)
{
    (void)cluster;
    kprintf("[FAT32]   '%s'  %u bytes%s\n",
            name, size, (attr & 0x10) ? "  <DIR>" : "");
}
// =====================================================================
// >>> FIN BLOQUE DE TEST FAT32 <<<
// =====================================================================

// =====================================================================
// Callback del menu de inicio
// =====================================================================
static int next_term_x = 60;
static int next_term_y = 60;
static int next_term_n = 1;

static void start_action(int action) {
    switch (action) {
        case 1: {
            /* Nueva terminal, con posicion escalonada */
            char title[32];
            title[0] = 'T'; title[1] = 'e'; title[2] = 'r'; title[3] = 'm';
            title[4] = 'i'; title[5] = 'n'; title[6] = 'a'; title[7] = 'l';
            title[8] = ' '; title[9] = '0' + (next_term_n % 10); title[10] = 0;
            shell_new_terminal(next_term_x, next_term_y, 420, 280, title);
            next_term_x += 30;
            next_term_y += 30;
            if (next_term_x > 400) { next_term_x = 60; next_term_y = 60; }
            next_term_n++;
            composer_force_redraw();
            break;
        }
        case 2:
            kprintf("menu: Acerca de MiniSO v8.3\n");
            break;
        case 3:
            kprintf("menu: Reiniciar\n");
            break;
        case 4:
            kprintf("menu: Cerrar todo\n");
            break;
        default:
            break;
    }
}

// =====================================================================
// Dormir la CPU
// =====================================================================
static inline void cpu_idle_wait(void) {
    __asm__ __volatile__("sti; hlt");
}

// =====================================================================
// Escritorio principal
// =====================================================================
void desktop_run(void) {
    if (!gfx_ready()) {
        kprintf("desktop: gfx no disponible\n");
        return;
    }
    if (!gfx_backbuffer_init())
        kprintf("desktop: AVISO sin backbuffer\n");

    int r = mouse_init();
    kprintf("desktop: mouse_init = %d\n", r);

    /* ---------- Compositor ---------- */
    composer_init();

    /* ---------- Shell ---------- */
    shell_init();

    /* ---------- Menu de inicio ---------- */
    taskbar_start_init(start_action);

    /* ---------- Primera terminal ---------- */
    int wid = shell_new_terminal(60, 60, 420, 280, "Terminal 1");
    composer_set_terminal(wid);
    composer_force_redraw();

    // =================================================================
    // >>> INICIO LLAMADA DE TEST A FAT32 <<<
    // Esta llamada es SOLO para probar fat32_win.cpp.
    // Cuando fat32 funcione, BORRA este bloque completo
    // (desde aqui hasta ">>> FIN LLAMADA DE TEST A FAT32 <<<").
    // =================================================================
    kprintf("[TEST] Probando fat32_mount(%u)...\n", FAT32_TEST_LBA);
    int fat32_ok = fat32_mount(FAT32_TEST_LBA);
    kprintf("[TEST] fat32_mount devolvio %d\n", fat32_ok);
    if (fat32_ok) {
        kprintf("[TEST] FAT32 montado correctamente\n");
        kprintf("[TEST] Listando / ...\n");
        int n = fat32_list("/", fat32_test_list_cb);
        kprintf("[TEST] fat32_list devolvio %d\n", n);
    } else {
        kprintf("[TEST] FAT32 NO se pudo montar\n");
    }
    // =================================================================
    // >>> FIN LLAMADA DE TEST A FAT32 <<<
    // =================================================================

    while (1) {
        int did_work = 0;
        unsigned int now = pit_get_ticks();

        /* ---------- Reloj + CPU ---------- */
        if (composer_periodic(now)) did_work = 1;

        /* ---------- Raton ---------- */
        if (composer_handle_mouse()) did_work = 1;

        /* ---------- Teclado: enviar a la ventana enfocada si es terminal ---------- */
        char c = keyboard_getchar();
        if (c) {
            int fid = win_get_focused();
            if (fid >= 0 && shell_is_terminal(fid)) {
                shell_handle_key(fid, c);
                composer_dirty_window(fid);
            }
            did_work = 1;
        }

        /* ---------- Aplicar dirty ---------- */
        if (composer_has_dirty()) {
            composer_apply();
            did_work = 1;
        }

        /* ---------- Dormir o ceder ---------- */
        if (!did_work) {
            cpu_monitor_sleep_start();
            cpu_idle_wait();
            cpu_monitor_sleep_end();
        } else {
            thread_yield();
        }
    }
}

// =====================================================================
// Stubs para screen.cpp
//
// term_wid, term_prompt y dirty_union son simbolos que screen.cpp
// usa para redirigir la salida al terminal grafico. Como no tenemos
// terminal.cpp con esas definiciones, las dejamos como stubs.
// En la practica nunca se ejecutan porque screen_set_terminal_mode(1)
// no se llama desde el kernel.
// =====================================================================

extern "C" int term_wid = -1;

extern "C" void term_prompt(void) {
    /* stub: el compositor dibuja el prompt por su cuenta */
}

extern "C" void dirty_union(int x, int y, int w, int h) {
    /* stub: el compositor se encarga del redibujado */
    (void)x; (void)y; (void)w; (void)h;
}
