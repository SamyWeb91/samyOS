#include "desktop.h"
#include "gfx.h"
#include "screen.h"
#include "rtc.h"
#include "pit.h"
#include "keyboard.h"
#include "thread.h"

// =====================================================================
// Colores del escritorio
// =====================================================================
#define COLOR_DESKTOP_BG     0x0087CEEB   // azul claro (skyblue)
#define COLOR_TASKBAR_BG     0x001A3A5C   // azul oscuro
#define COLOR_TASKBAR_TEXT   0x00FFFFFF   // blanco
#define COLOR_TASKBAR_BORDER 0x00000000   // negro

// Dimensiones
#define TASKBAR_HEIGHT  28
#define CLOCK_RIGHT     10   // margen derecho del reloj
#define CLOCK_WIDTH     80   // ancho aproximado para 8 caracteres "HH:MM:SS"

// =====================================================================
// Helpers
// =====================================================================
static void draw_taskbar(void) {
    int W = gfx_width();
    int H = gfx_height();

    // Fondo de la barra
    gfx_fill_rect(0, H - TASKBAR_HEIGHT, W, TASKBAR_HEIGHT, COLOR_TASKBAR_BG);

    // Línea superior de la barra
    gfx_fill_rect(0, H - TASKBAR_HEIGHT, W, 1, COLOR_TASKBAR_BORDER);
}

static void draw_clock(void) {
    int W = gfx_width();
    int H = gfx_height();

    RTCTime t;
    rtc_read(&t);

    // Formato "HH:MM:SS"
    char buf[9];
    buf[0] = '0' + (t.hour / 10);
    buf[1] = '0' + (t.hour % 10);
    buf[2] = ':';
    buf[3] = '0' + (t.minute / 10);
    buf[4] = '0' + (t.minute % 10);
    buf[5] = ':';
    buf[6] = '0' + (t.second / 10);
    buf[7] = '0' + (t.second % 10);
    buf[8] = 0;

    // Posición: a la derecha de la barra
    int x = W - CLOCK_RIGHT - 8 * 8;   // 8 chars × 8 px
    int y = H - TASKBAR_HEIGHT + (TASKBAR_HEIGHT - 16) / 2;

    // Borrar área del reloj (por si cambió)
    gfx_fill_rect(x - 4, H - TASKBAR_HEIGHT + 1,
                  8 * 8 + 8, TASKBAR_HEIGHT - 1, COLOR_TASKBAR_BG);

    // Dibujar el reloj
    gfx_draw_string(x, y, buf, COLOR_TASKBAR_TEXT, COLOR_TASKBAR_BG);
}

// =====================================================================
// Escritorio
// =====================================================================
void desktop_run(void) {
    if (!gfx_ready()) {
        kprintf("desktop: gfx no disponible\n");
        return;
    }

    int W = gfx_width();
    int H = gfx_height();

    // 1. Fondo azul claro (todo menos la barra)
    gfx_fill_rect(0, 0, W, H - TASKBAR_HEIGHT, COLOR_DESKTOP_BG);

    // 2. Barra de tareas
    draw_taskbar();

    // 3. Reloj inicial
    draw_clock();

    // 4. Mensaje de bienvenida en el escritorio
    gfx_draw_string(20, 20, "MiniOS Desktop", 0x00FFFFFF, COLOR_DESKTOP_BG);
    gfx_draw_string(20, 40, "Pulsa ESC para volver a la shell.",
                    0x00FFFFFF, COLOR_DESKTOP_BG);

    // 5. Bucle principal: actualizar reloj cada segundo, ESC para salir
    unsigned int last_tick = pit_get_ticks();

    while (1) {
        // Leer teclado (no bloqueante)
        char c = keyboard_getchar();
        if (c == 27) {   // ESC
            break;
        }

        // Actualizar reloj cada ~1 segundo (100 ticks a 100 Hz)
        unsigned int now = pit_get_ticks();
        if (now - last_tick >= 100) {
            last_tick = now;
            draw_clock();
        }

        // Ceder CPU
        thread_yield();
    }

    // Al salir, restaurar el fondo del escritorio a la shell
    screen_clear();
}
