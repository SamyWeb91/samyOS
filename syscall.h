#pragma once

#define SYS_EXIT     0
#define SYS_PRINT    1
#define SYS_PUTCHAR  2
#define SYS_GETCHAR  3
#define SYS_TICKS    4

// Punto de entrada para el handler de INT 0x80
extern "C" unsigned int syscall_handler(unsigned int num,
                                         unsigned int a, unsigned int b,
                                         unsigned int c);
