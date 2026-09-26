#include "syscall.h"
#include "screen.h"
#include "keyboard.h"
#include "pit.h"

// Variable global: si el programa llama a exit, guardamos aquí la señal
volatile unsigned int program_exited = 0;

extern "C" unsigned int syscall_handler(unsigned int num,
                                         unsigned int a, unsigned int b,
                                         unsigned int c) {
    (void)b;
    (void)c;
    switch (num) {
        case SYS_EXIT:
            program_exited = 1;
            return 0;
        case SYS_PRINT:
            screen_write((const char*)a);
            return 0;
        case SYS_PUTCHAR:
            screen_putchar((char)a);
            return 0;
        case SYS_GETCHAR:
            return (unsigned int)(unsigned char)keyboard_getchar();
        case SYS_TICKS:
            return pit_get_ticks();
        default:
            return 0;
    }
}

void syscall_reset() {
    program_exited = 0;
}
