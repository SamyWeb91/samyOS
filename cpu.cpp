// cpu.cpp
#include "cpu.h"
#include "pit.h"
#include "screen.h"

static unsigned long long tsc_freq = 0;        // ciclos por segundo
static unsigned long long window_start = 0;    // TSC al inicio de la ventana
static unsigned long long iter_start = 0;      // TSC al inicio de la iteración
static unsigned long long busy_cycles = 0;     // ciclos ocupados en la ventana
static unsigned long long total_cycles = 0;    // ciclos totales en la ventana
static int                last_usage = 0;

static inline unsigned long long rdtsc(void) {
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long)hi << 32) | lo;
}

void cpu_init(void) {
    // Calibrar: contar ciclos de TSC durante 100 ms de PIT
    unsigned int t0 = pit_get_ticks();
    unsigned long long c0 = rdtsc();
    while (pit_get_ticks() - t0 < 100) { /* espera */ }
    unsigned long long c1 = rdtsc();

    // 100 ticks de PIT = 1 segundo (si PIT está a 100 Hz)
    tsc_freq = c1 - c0;

    window_start = rdtsc();
    kprintf("CPU: TSC freq aprox %u MHz\n", (unsigned int)(tsc_freq / 1000000));
}

void cpu_begin_work(void) {
    iter_start = rdtsc();
}

void cpu_end_work(int busy) {
    unsigned long long now = rdtsc();
    unsigned long long dt = now - iter_start;

    total_cycles += dt;
    if (busy) busy_cycles += dt;

    // Cada segundo, calcular uso
    if (now - window_start >= tsc_freq) {
        if (total_cycles > 0)
            last_usage = (int)((busy_cycles * 100) / total_cycles);
        else
            last_usage = 0;

        busy_cycles = 0;
        total_cycles = 0;
        window_start = now;
    }
}

int cpu_usage(void) {
    return last_usage;
}
