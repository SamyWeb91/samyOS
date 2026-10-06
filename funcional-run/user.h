#pragma once

#include <stdint.h>

// Direcciones en el espacio del usuario
#define USER_CODE_VADDR   0x400000
#define USER_STACK_VADDR  0x402000
#define USER_STACK_SIZE   0x2000     // 8 KB
#define USER_MSG_VADDR    0x400800
#define USER_PROG_MAX     4096       // tamaño máximo binario

#ifdef __cplusplus
extern "C" {
#endif

void user_init(void);                 // mapea código + stack del usuario
void user_start(void);                // entra en ring 3 (blob del kernel)
uint32_t user_get_kernel_stack(void); // stack del kernel para TSS.esp0

// Ejecutar un buffer en ring 3.
// Copia `size` bytes desde `buf` a 0x400000 y hace iret a ring 3.
// Bloquea hasta que el binario hace exit().
int user_run_buffer(const void *buf, uint32_t size);

// Leer 4 KB del LBA indicado y ejecutarlos en ring 3.
int user_load_and_run(uint32_t lba);

#ifdef __cplusplus
}
#endif
