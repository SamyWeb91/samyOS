#include "screen.h"
#include "paging.h"
#include "tss.h"

#include "ring3_test_blob.c" 

// MOVEMOS EL BINARIO A 128 MB (Fuera del rango de identidad del kernel)
#define USER_CODE  0x08000000
#define USER_STACK 0x08010000

extern "C" void enter_user_mode(uint32_t entry, uint32_t user_stack);
extern "C" void paging_map(unsigned int virt, unsigned int phys, unsigned int flags);

static uint8_t ring3_kernel_stack[8192] __attribute__((aligned(16)));

extern "C" void ring3_test_run(void) {
    kprintf("ring3: mapeando code y stack en 0x%x con PTE_USER...\n", USER_CODE);

    // 1. Mapear la pagina del codigo (usando memoria fisica nueva)
    // Usamos la direccion fisica 0x500000 (5 MB), que esta libre.
    paging_map(USER_CODE, 0x500000, PTE_PRESENT | PTE_RW | PTE_USER);
    
    // 2. Mapear la pila
    paging_map(USER_STACK, 0x501000, PTE_PRESENT | PTE_RW | PTE_USER);
    paging_map(USER_STACK + 0x1000, 0x502000, PTE_PRESENT | PTE_RW | PTE_USER);

    // 3. Copiar el binario a su memoria fisica
    uint8_t *dst = (uint8_t *)0x500000; // Escribimos directo a la fisica
    const uint8_t *src = ring3_test_bin;
    for (unsigned int i = 0; i < ring3_test_bin_len; i++) {
        dst[i] = src[i];
    }

    // 4. Copiar el mensaje a la memoria fisica (offset 0x800)
    const char *msg = "Hola desde RING 3!\n";
    uint8_t *msg_dst = (uint8_t *)(0x500000 + 0x800);
    for (int i = 0; i < 18; i++) {
        msg_dst[i] = msg[i];
    }

    // 5. Flush TLB
    paging_flush_tlb();

    // 6. TSS.esp0
    uint32_t kstack = (uint32_t)ring3_kernel_stack + sizeof(ring3_kernel_stack);
    tss_set_kernel_stack(kstack);

    kprintf("ring3: saltando a 0x%x (ESP=0x%x)\n", USER_CODE, USER_STACK + 0x2000);

    // 7. Saltar a ring 3
    enter_user_mode(USER_CODE, USER_STACK + 0x2000);

    kprintf("ring3: volvimos de ring 3\n");
}
