/*
 * Copyright (C) 2026 Samuel Tamayo Infante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "isr.h"   // para Regs

/* =====================================================================
 * Numeros de syscall — numeracion Linux i386
 *
 * Se usa la numeracion de Linux para las syscalls POSIX, de forma que
 * si algun dia portamos codigo que haga `int 0x80` con los numeros
 * de Linux, funcione sin cambios. Las syscalls propias de MiniSO
 * (si las hay en el futuro) iran a partir del 200.
 * ===================================================================== */

/* --- POSIX (numeracion Linux i386) --- */
#define SYS_EXIT     1
#define SYS_READ     3
#define SYS_WRITE    4
#define SYS_OPEN     5
#define SYS_CLOSE    6
#define SYS_EXECVE   11
#define SYS_CHDIR    12
#define SYS_GETPID   20
#define SYS_KILL     37
#define SYS_BRK      45
#define SYS_MMAP     90
#define SYS_NANOSLEEP 162
#define SYS_DRAW_RECT 11
/* --- Propias de MiniSO (reservado, a partir del 200) --- */
/* #define SYS_MINIOS_XXX  200 */

/* =====================================================================
 * Declaracion de syscalls (estilo POSIX, adaptado de ToaruOS)
 *
 * Estas macros generan el prototipo de la funcion que userland usa
 * para invocar la syscall. Cada una se implementa con un `int 0x80`
 * en un archivo aparte (por ejemplo syscall_user.cpp o un .asm).
 * ===================================================================== */
#define DECL_SYSCALL0(fn)               long sys_##fn(void)
#define DECL_SYSCALL1(fn,p1)            long sys_##fn(p1)
#define DECL_SYSCALL2(fn,p1,p2)         long sys_##fn(p1,p2)
#define DECL_SYSCALL3(fn,p1,p2,p3)      long sys_##fn(p1,p2,p3)
#define DECL_SYSCALL4(fn,p1,p2,p3,p4)   long sys_##fn(p1,p2,p3,p4)
#define DECL_SYSCALL5(fn,p1,p2,p3,p4,p5) \
                                        long sys_##fn(p1,p2,p3,p4,p5)
#define DECL_SYSCALL6(fn,p1,p2,p3,p4,p5,p6) \
                                        long sys_##fn(p1,p2,p3,p4,p5,p6)

/* --- Declaraciones (prototipos para userland) --- */
DECL_SYSCALL1(exit,      int);
DECL_SYSCALL3(read,      int, void *, unsigned int);
DECL_SYSCALL3(write,     int, const void *, unsigned int);
DECL_SYSCALL3(open,      const char *, int, int);
DECL_SYSCALL1(close,     int);
DECL_SYSCALL3(execve,    const char *, char *const *, char *const *);
DECL_SYSCALL1(chdir,     const char *);
DECL_SYSCALL0(getpid);
DECL_SYSCALL2(kill,      int, int);
DECL_SYSCALL1(brk,       void *);
DECL_SYSCALL6(mmap,      void *, unsigned int, int, int, int, int);
DECL_SYSCALL2(nanosleep, const void *, void *);

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
