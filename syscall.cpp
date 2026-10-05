#include "syscall.h"
#include "screen.h"
#include "idt.h"
#include "thread.h"

// Declarado en syscall.asm
extern "C" void isr_128();

// =====================================================================
// Registrar la entrada 0x80 en la IDT
// =====================================================================
extern "C" void syscall_init(void) {
    // flags 0xEE = present, DPL=3 (para que ring 3 pueda llamar),
    //              tipo = 0xE (interrupt gate 32-bit)
    idt_set_gate(0x80, (unsigned int)isr_128, 0x08, 0xEE);
    kprintf("Syscall: int 0x80 registrado (DPL=3)\n");
}

// =====================================================================
// Dispatcher
// =====================================================================
extern "C" void syscall_handler(Regs *r) {
    unsigned int num = r->eax;

    switch (num) {
        case SYS_WRITE: {
            // args: ebx = ptr, ecx = len
            const char *str = (const char *)r->ebx;
            unsigned int len = r->ecx;
            for (unsigned int i = 0; i < len; i++) {
                screen_putchar(str[i]);
            }
            r->eax = len;   // valor de retorno
            break;
        }
        case SYS_EXIT: {
            // Marcar el hilo actual como terminado
            kprintf("\n[Syscall: exit(%u)]\n", r->ebx);
            thread_exit();
            // no llega aquí
            break;
        }
        default:
            kprintf("[Syscall desconocida: %u]\n", num);
            r->eax = (unsigned int)-1;
            break;
    }
}
