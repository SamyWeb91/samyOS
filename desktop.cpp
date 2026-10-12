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

// =====================================================================
// Helpers de string
// =====================================================================
static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

// =====================================================================
// Callbacks para screen.cpp
// =====================================================================
extern "C" void desktop_terminal_putchar(char c) { (void)c; }
extern "C" void desktop_terminal_clear(void) { }

// =====================================================================
// Callback del menu de inicio
// =====================================================================
static void start_action(int action) {
    switch (action) {
        case 1: kprintf("menu: Terminal\n");    break;
        case 2: kprintf("menu: Acerca de\n");   break;
        case 3: kprintf("menu: Reiniciar\n");   break;
        case 4: kprintf("menu: Cerrar todo\n"); break;
        default: break;
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

    /* ---------- Menu de inicio ---------- */
    taskbar_start_init(start_action);

    /* ---------- 3 ventanas de prueba ---------- */
    int w1 = win_sys_create(60,  60, 300, 200, "Ventana 1");
    int w2 = win_sys_create(400, 80, 300, 200, "Ventana 2");
    int w3 = win_sys_create(200, 240, 300, 200, "Ventana 3");

    kprintf("desktop: ventanas creadas w1=%d w2=%d w3=%d\n", w1, w2, w3);

    if (w1 >= 0) {
        win_sys_write(w1, "Ventana de prueba 1\n", 20);
        win_sys_write(w1, "Arrastrame por la barra azul\n", 29);
    }
    if (w2 >= 0) {
        win_sys_write(w2, "Ventana de prueba 2\n", 20);
        win_sys_write(w2, "Click en la X para cerrar\n", 26);
    }
    if (w3 >= 0) {
        win_sys_write(w3, "Ventana de prueba 3\n", 20);
        win_sys_write(w3, "El raton debe moverse suave\n", 28);
    }

    composer_force_redraw();

    while (1) {
        int did_work = 0;
        unsigned int now = pit_get_ticks();

        /* ---------- Reloj + CPU: refresco periodico del compositor ---------- */
        if (composer_periodic(now)) did_work = 1;

        /* ---------- Raton: cursor + click + drag + menu ---------- */
        if (composer_handle_mouse()) did_work = 1;

        /* ---------- Teclado: no hacemos nada, solo consumir ---------- */
        char c = keyboard_getchar();
        if (c) {
            (void)c;
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
