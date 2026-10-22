#include "screen.h"
#include "paging.h"
#include "tss.h"

#include "ring3_test_blob.c"

// Usamos 0x08000000 (128 MB), que está fuera del rango del kernel
#define USER_CODE  0x08000000
#define USER_MSG   0x08000800
#define USER_STACK 0x08010000

extern "C" void enter_user_mode(uint32_t entry, uint32_t user_stack);
extern "C" void paging_map(unsigned int virt, unsigned int phys, unsigned int flags);
extern "C" void paging_flush_tlb(void);

static uint8_t ring3_kernel_stack[8192] __attribute__((aligned(16)));

extern "C" void ring3_test_run(void) {
    kprintf("ring3: mapeando en 0x%x con PTE_USER...\n", USER_CODE);

    // Mapear 1 pagina para el codigo en 0x08000000 -> fisica 0x500000
    paging_map(USER_CODE, 0x500000, PTE_PRESENT | PTE_RW | PTE_USER);
    // Mapear 1 pagina para el mensaje en 0x08000800 -> fisica 0x501000
    paging_map(USER_CODE + 0x1000, 0x501000, PTE_PRESENT | PTE_RW | PTE_USER);
    // Mapear 2 paginas para la pila
    paging_map(USER_STACK, 0x502000, PTE_PRESENT | PTE_RW | PTE_USER);
    paging_map(USER_STACK + 0x1000, 0x503000, PTE_PRESENT | PTE_RW | PTE_USER);

    // FIX: Limitar el tamaño de copia para evitar desbordar la memoria
    // si ring3_test_bin_len contiene basura (por ejemplo, por un error en xxd)
    unsigned int safe_len = ring3_test_bin_len;
    if (safe_len == 0 || safe_len > 4096) {
        kprintf("ring3: ERROR tamano binario invalido (%u)\n", safe_len);
        return;
    }
    kprintf("ring3: copiando %u bytes...\n", safe_len);

    // Copiar el binario a la memoria fisica 0x500000
    uint8_t *dst = (uint8_t *)0x500000;
    const uint8_t *src = ring3_test_bin;
    for (unsigned int i = 0; i < safe_len; i++) dst[i] = src[i];

    // Copiar el mensaje a la fisica 0x501000 + 0x800 (offset 0x800)
    const char *msg = "Hola desde RING 3!\n";
    uint8_t *msg_dst = (uint8_t *)(0x501000 + 0x800);
    for (int i = 0; i < 18; i++) msg_dst[i] = msg[i];

    paging_flush_tlb();

    uint32_t kstack = (uint32_t)ring3_kernel_stack + sizeof(ring3_kernel_stack);
    tss_set_kernel_stack(kstack);

    kprintf("ring3: saltando a 0x%x (ESP=0x%x)\n", USER_CODE, USER_STACK + 0x2000);

    enter_user_mode(USER_CODE, USER_STACK + 0x2000);

    kprintf("ring3: volvimos de ring 3\n");
}
