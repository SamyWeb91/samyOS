#include "screen.h"
#include "paging.h"
#include "tss.h"

#include "ring3_test_blob.c"

// =====================================================================
// Direcciones virtuales del payload y su pila
// =====================================================================
// Código en 0x08000000 (128 MB): fuera del rango del kernel.
#define USER_CODE       0x08000000u
// Pila en 0x08010000 (128 MB + 64 KB): crece hacia abajo.
// Mapeamos 16 KB (4 páginas) para tener margen.
#define USER_STACK_BASE 0x08010000u
#define USER_STACK_SIZE 0x4000u                       // 16 KB
#define USER_STACK_TOP  (USER_STACK_BASE + USER_STACK_SIZE)

// =====================================================================
// Direcciones físicas donde copiamos el payload.
// ¡OJO! Deben estar libres. Si tu kernel ocupa < 8 MB, 0x800000
// en adelante está libre. Ajusta si tu layout es distinto.
// =====================================================================
#define PHYS_CODE       0x00800000u   // 8 MB
#define PHYS_STACK      0x00810000u   // 8 MB + 64 KB (16 KB de pila = 4 pages)

// Tamaño máximo del payload (1 página de 4 KB)
#define MAX_PAYLOAD     0x1000u

// =====================================================================
// Pila del kernel dedicada para TSS.esp0
// Estática en .bss: garantizada mapeada, 8 KB de espacio.
// =====================================================================
static uint8_t ring3_kernel_stack[8192] __attribute__((aligned(16)));

extern "C" void enter_user_mode(uint32_t entry, uint32_t user_stack);
extern "C" void paging_map(unsigned int virt, unsigned int phys, unsigned int flags);
extern "C" void paging_flush_tlb(void);

// =====================================================================
// Helper: mapear un rango virtual -> físico con flags
// =====================================================================
static void map_range(uint32_t virt, uint32_t phys, uint32_t size, uint32_t flags) {
    for (uint32_t off = 0; off < size; off += 0x1000) {
        paging_map(virt + off, phys + off, flags);
    }
}

// =====================================================================
// ring3_test_run — entrada desde la shell
// =====================================================================
extern "C" void ring3_test_run(void) {
    kprintf("ring3: === DIAGNOSTICO ===\n");

    // -----------------------------------------------------------------
    // 0. Sanity check del blob
    // -----------------------------------------------------------------
    unsigned int safe_len = ring3_test_bin_len;
    if (safe_len == 0 || safe_len > MAX_PAYLOAD) {
        kprintf("ring3: ERROR tamano binario invalido (%u, max=%u)\n",
                safe_len, MAX_PAYLOAD);
        return;
    }
    kprintf("ring3: blob = %u bytes\n", safe_len);

    // -----------------------------------------------------------------
    // 1. Mapear código de usuario: USER_CODE -> PHYS_CODE
    // -----------------------------------------------------------------
    // Una sola página es suficiente para MAX_PAYLOAD=4KB.
    paging_map(USER_CODE, PHYS_CODE, PTE_PRESENT | PTE_RW | PTE_USER);

    // -----------------------------------------------------------------
    // 2. Mapear pila de usuario: USER_STACK_BASE -> PHYS_STACK
    //    16 KB = 4 páginas.
    // -----------------------------------------------------------------
    map_range(USER_STACK_BASE, PHYS_STACK, USER_STACK_SIZE,
              PTE_PRESENT | PTE_RW | PTE_USER);

    // -----------------------------------------------------------------
    // 3. Copiar el binario al page físico mapeado
    // -----------------------------------------------------------------
    kprintf("ring3: copiando %u bytes a virt=0x%x (phys=0x%x)...\n",
            safe_len, USER_CODE, PHYS_CODE);

    uint8_t *dst = (uint8_t *)USER_CODE;   // escribimos via virtual
    const uint8_t *src = ring3_test_bin;
    for (unsigned int i = 0; i < safe_len; i++) dst[i] = src[i];

    // Flush de TLB tras mapear y copiar
    paging_flush_tlb();

    // -----------------------------------------------------------------
    // 4. Verificación: leer de vuelta el primer byte del código
    //    (si esto falla, es #PF y el handler lo reportará)
    // -----------------------------------------------------------------
    volatile uint8_t verify = *(volatile uint8_t *)USER_CODE;
    kprintf("ring3: verificacion OK (primer byte = 0x%x)\n", verify);

    // -----------------------------------------------------------------
    // 5. Configurar TSS.esp0 con NUESTRA pila dedicada (no 0x90000)
    // -----------------------------------------------------------------
    uint32_t esp0 = (uint32_t)(ring3_kernel_stack + sizeof(ring3_kernel_stack));
    esp0 &= ~0xFu;   // alinear a 16 bytes
    kprintf("ring3: TSS.esp0 = 0x%x (pila dedicada, %u bytes)\n",
            esp0, (unsigned)sizeof(ring3_kernel_stack));
    tss_set_kernel_stack(esp0);

    // -----------------------------------------------------------------
    // 6. Saltar a ring 3
    //    ESP = USER_STACK_TOP (tope de la pila, que SÍ está mapeada)
    // -----------------------------------------------------------------
    kprintf("ring3: saltando a EIP=0x%x (ESP=0x%x)...\n",
            USER_CODE, USER_STACK_TOP);

    enter_user_mode(USER_CODE, USER_STACK_TOP);

    // -----------------------------------------------------------------
    // 7. Solo llegamos aquí si el payload hizo exit (int 0x80) y el
    //    handler de syscall lo devolvió al kernel. Si el payload es
    //    un bucle infinito, NUNCA volvemos.
    // -----------------------------------------------------------------
    kprintf("ring3: volvimos de ring 3 (el binario termino)\n");
}
