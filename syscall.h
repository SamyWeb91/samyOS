#pragma once

#include "isr.h"   // para Regs

// Números de syscall
#define SYS_WRITE  1
#define SYS_EXIT   2

#ifdef __cplusplus
extern "C" {
#endif

void syscall_init(void);          // registra int 0x80 en la IDT
void syscall_handler(Regs *r);    // dispatcher

#ifdef __cplusplus
}
#endif
