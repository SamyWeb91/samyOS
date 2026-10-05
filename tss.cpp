#include "tss.h"
#include "gdt.h"
#include "screen.h"

static TSS tss;

extern "C" void tss_flush(void);   // en tss.asm

// =====================================================================
// Init
// =====================================================================
extern "C" void tss_init(void) {
    // Limpiar la estructura
    uint8_t *p = (uint8_t*)&tss;
    for (unsigned int i = 0; i < sizeof(TSS); i++) p[i] = 0;

    // ss0 = 0x10 (segmento de datos del kernel)
    tss.ss0 = 0x10;

    // esp0 = 0 por ahora. Se configura en cada cambio de proceso (futuro ring 3).
    tss.esp0 = 0;

    // iomap_base = sizeof(TSS) → sin bitmap de I/O
    // (si fuera menor, la CPU leería basura y daría #GP al hacer I/O)
    tss.iomap_base = sizeof(TSS);

    // Registrar el descriptor en la GDT.
    // Índice 5 → selector 0x28.
    gdt_set_tss(5, (uint32_t)&tss, sizeof(TSS) - 1);

    // Cargar el TSS en el registro TR
    tss_flush();

    kprintf("TSS: cargado en 0x28 (base=%x, limit=%u, ss0=0x%x)\n",
            (uint32_t)&tss, sizeof(TSS) - 1, tss.ss0);
}

// =====================================================================
// Set kernel stack (para ring 3)
// =====================================================================
extern "C" void tss_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}

// =====================================================================
// Getters (para shell)
// =====================================================================
extern "C" uint32_t tss_get_esp0(void)  { return tss.esp0; }
extern "C" uint32_t tss_get_ss0(void)   { return tss.ss0; }
extern "C" uint32_t tss_get_base(void)  { return (uint32_t)&tss; }
extern "C" uint32_t tss_get_limit(void) { return sizeof(TSS) - 1; }
