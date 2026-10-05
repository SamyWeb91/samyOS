#pragma once

#include <stdint.h>

// Direcciones en el espacio del usuario
#define USER_CODE_VADDR   0x400000
#define USER_STACK_VADDR  0x402000
#define USER_STACK_SIZE   0x2000     // 8 KB
#define USER_MSG_VADDR    0x400800

#ifdef __cplusplus
extern "C" {
#endif

void user_init(void);                 // mapea código + stack del usuario
void user_start(void);                // entra en ring 3
uint32_t user_get_kernel_stack(void); // devuelve un stack del kernel para el TSS

#ifdef __cplusplus
}
#endif
