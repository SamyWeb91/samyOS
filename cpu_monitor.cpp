// cpu_monitor.cpp
#include "cpu_monitor.h"
#include "pit.h"

/* PIT a 100 Hz → 1 segundo = 100 ticks.
   Si tu PIT va a otra frecuencia, ajusta esta constante. */
#define PIT_HZ 100

static unsigned int window_start = 0;   /* tick al inicio de la ventana */
static unsigned int total_ticks  = 0;   /* ticks transcurridos en la ventana */
static unsigned int sleep_ticks  = 0;   /* ticks dormidos en la ventana */
static unsigned int sleep_start  = 0;   /* tick en el que entramos en hlt */
static int          usage_pct    = 0;   /* último % calculado */

/* ---- API ---- */

extern "C" void cpu_monitor_init(void) {
    window_start = pit_get_ticks();
    total_ticks  = 0;
    sleep_ticks  = 0;
    sleep_start  = 0;
    usage_pct    = 0;
}

extern "C" void cpu_monitor_begin_iter(void) {
    /* Nada especial: el tiempo lo medimos por ticks de PIT, no por
       iteraciones. Esta función existe por simetría con end_iter. */
}

extern "C" void cpu_monitor_sleep_start(void) {
    sleep_start = pit_get_ticks();
}

extern "C" void cpu_monitor_sleep_end(void) {
    unsigned int now = pit_get_ticks();
    sleep_ticks += (now - sleep_start);
}

extern "C" void cpu_monitor_end_iter(void) {
    unsigned int now = pit_get_ticks();
    unsigned int elapsed = now - window_start;

    if (elapsed >= (unsigned int)PIT_HZ) {
        total_ticks = elapsed;

        /* El tiempo dormido no puede superar el total. */
        if (sleep_ticks > total_ticks) sleep_ticks = total_ticks;

        unsigned int busy = total_ticks - sleep_ticks;
        usage_pct = (int)((busy * 100u) / total_ticks);
        if (usage_pct < 0)   usage_pct = 0;
        if (usage_pct > 100) usage_pct = 100;

        /* Reiniciar ventana */
        window_start = now;
        total_ticks  = 0;
        sleep_ticks  = 0;
    }
}

extern "C" int cpu_monitor_usage(void) {
    return usage_pct;
}
