// =====================================================================
// user.cpp
//
// Loader del kernel para ring 3.
// - Mapea el espacio del usuario (0x400000-0x408000) con PTE_USER.
// - Copia el blob del programa (user_prog_blob.o) a 0x400000.
// - Copia el mensaje a 0x400800.
// - Provee user_start() para entrar a ring 3.
// =====================================================================

#include "user.h"
#include "screen.h"
#include "paging.h"

// Simbolos exportados por user_prog_blob.o (via objcopy)
extern "C" uint8_t user_prog_start[];
extern "C" uint8_t user_prog_end[];

// Stack del kernel dedicado al user (para int 0x80)
static uint8_t user_kernel_stack[8192] __attribute__((aligned(16)));

extern "C" void enter_user_mode(uint32_t entry, uint32_t user_stack);

// =====================================================================
// Mapear el espacio del usuario y copiar el blob
// =====================================================================
extern "C" void user_init(void) {
    paging_map_range(0x400000, 0x400000, 32,
                     PTE_PRESENT | PTE_RW | PTE_USER);

    kprintf("User: mapeado 0x400000 - 0x408000 (PTE_USER)\n");

    uint32_t size = (uint32_t)user_prog_end - (uint32_t)user_prog_start;
    uint8_t *dst = (uint8_t *)USER_CODE_VADDR;
    uint8_t *src = user_prog_start;
    for (uint32_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }
    kprintf("User: copiados %u bytes a 0x%x\n",
            size, USER_CODE_VADDR);

    const char *msg = "Hola desde RING 3!\n";
    uint8_t *m = (uint8_t *)USER_MSG_VADDR;
    int i = 0;
    while (msg[i]) { m[i] = msg[i]; i++; }
    m[i] = 0;
    kprintf("User: mensaje copiado a 0x%x\n", USER_MSG_VADDR);
}

// =====================================================================
// Entrar en ring 3
// =====================================================================
extern "C" void user_start(void) {
    uint32_t user_stack_top = USER_STACK_VADDR + USER_STACK_SIZE;

    kprintf("User: entrando a ring 3 (EIP=%x, ESP=%x)\n",
            USER_CODE_VADDR, user_stack_top);

    enter_user_mode(USER_CODE_VADDR, user_stack_top);

    kprintf("User: volvimos de ring 3?!\n");
}

// =====================================================================
// Stack del kernel para syscalls (TSS.esp0)
// =====================================================================
extern "C" uint32_t user_get_kernel_stack(void) {
    return (uint32_t)user_kernel_stack + sizeof(user_kernel_stack);
}
