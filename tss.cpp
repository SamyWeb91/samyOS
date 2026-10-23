#include "tss.h"
#include "gdt.h"
#include "screen.h"

static TSS tss;

extern "C" void tss_flush(void);   // en tss.asm

// =====================================================================
// Pila de emergencia para ring 0 (por si alguien olvida llamar a
// tss_set_kernel_stack antes de entrar a ring 3).
// Debe estar en una zona mapeada y escribible.
// =====================================================================
// FIX: pila de emergencia estática de 4 KB, alineada a 16 bytes
static uint8_t tss_emergency_stack[4096] __attribute__((aligned(16)));

// =====================================================================
// Init
// =====================================================================
extern "C" void tss_init(void) {
    // Limpiar la estructura
    uint8_t *p = (uint8_t*)&tss;
    for (unsigned int i = 0; i < sizeof(TSS); i++) p[i] = 0;

    // ss0 = 0x10 (segmento de datos del kernel)
    tss.ss0 = 0x10;

    // FIX: esp0 apunta a una pila de emergencia válida en vez de 0.
    // Si alguien intenta entrar a ring 3 sin llamar a
    // tss_set_kernel_stack(), la CPU usará esta pila y no explotará.
    tss.esp0 = (uint32_t)(tss_emergency_stack + sizeof(tss_emergency_stack));

    // FIX: iomap_base = 0xFFFF (valor canónico para "sin bitmap de I/O").
    // Con sizeof(TSS) algunos emuladores/CPUs reales pueden dar #GP
    // si iomap_base > limit del segmento TSS.
    tss.iomap_base = 0xFFFF;

    // Registrar el descriptor en la GDT.
    // Índice 5 → selector 0x28.
    gdt_set_tss(5, (uint32_t)&tss, sizeof(TSS) - 1);

    // Cargar el TSS en el registro TR
    tss_flush();

    kprintf("TSS: cargado en 0x28 (base=%x, limit=%u, ss0=0x%x, esp0=0x%x)\n",
            (uint32_t)&tss, sizeof(TSS) - 1, tss.ss0, tss.esp0);
}

// =====================================================================
// Set kernel stack (para ring 3)
// =====================================================================
extern "C" void tss_set_kernel_stack(uint32_t esp0) {
    // FIX: validación mínima. Si esp0 == 0, ignoramos la llamada y
    // mantenemos la pila de emergencia. Esto evita un triple fault
    // silencioso por un bug en el llamador.
    if (esp0 == 0) {
        kprintf("TSS: WARN tss_set_kernel_stack(0) ignorado\n");
        return;
    }
    tss.esp0 = esp0;
}

// =====================================================================
// FIX: helper para usar un buffer C++ como pila de kernel.
// Uso desde ring3_test.cpp:
//     tss_set_kernel_stack_from_array(ring3_kernel_stack,
//                                     sizeof(ring3_kernel_stack));
// =====================================================================
extern "C" void tss_set_kernel_stack_from_array(void *base, uint32_t size) {
    if (!base || size < 64) {
        kprintf("TSS: ERROR buffer de pila invalido\n");
        return;
    }
    uint32_t top = (uint32_t)base + size;
    top &= ~0xFu;   // alinear a 16 bytes (requisito de la ABI x86)
    tss.esp0 = top;
}

// =====================================================================
// Getters (para shell)
// =====================================================================
extern "C" uint32_t tss_get_esp0(void)  { return tss.esp0; }
extern "C" uint32_t tss_get_ss0(void)   { return tss.ss0; }
extern "C" uint32_t tss_get_base(void)  { return (uint32_t)&tss; }
extern "C" uint32_t tss_get_limit(void) { return sizeof(TSS) - 1; }
