#pragma once

#include "isr.h"   // para Regs

/* =====================================================================
 * Numeros de syscall (numeracion estilo Linux, para futuras ampliaciones)
 * ===================================================================== */
#define SYS_EXIT    1
#define SYS_WRITE   4
#define SYS_READ    3
/* ... futuras ... */

/* =====================================================================
 * Declaracion de syscalls (estilo POSIX, adaptado de ToaruOS)
 * ===================================================================== */
#define DECL_SYSCALL0(fn)              long sys_##fn(void)
#define DECL_SYSCALL1(fn,p1)           long sys_##fn(p1)
#define DECL_SYSCALL2(fn,p1,p2)        long sys_##fn(p1,p2)
#define DECL_SYSCALL3(fn,p1,p2,p3)     long sys_##fn(p1,p2,p3)

DECL_SYSCALL1(exit,  int);
DECL_SYSCALL2(write, int, const char *);

/* =====================================================================
 * API del kernel
 * ===================================================================== */
#ifdef __cplusplus
extern "C" {
#endif

void syscall_init(void);
void syscall_handler(Regs *r);

#ifdef __cplusplus
}
#endif
