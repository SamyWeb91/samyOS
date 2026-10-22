#include "isr.h"
#include "screen.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "thread.h"
#include "mouse.h"      // declara mouse_handler()

extern "C" void isr_handler(Regs *r) {
    // Excepciones (0-31)
    if (r->int_no < 32) {
        // --- MANEJO ESPECÍFICO DEL PAGE FAULT (Vector 14) ---
        if (r->int_no == 14) {
            unsigned int cr2;
            asm volatile("mov %%cr2, %0" : "=r"(cr2));
            kprintf("\n[PAGE FAULT] EIP=0x%x CR2=0x%x ERR=0x%x\n", 
                    r->eip, cr2, r->err_code);
            
            // Bucle infinito para que QEMU no haga Triple Fault
            // y podamos leer el error en la pantalla VNC.
            while (1) {
                asm volatile("cli; hlt");
            }
        }

        // --- OTRAS EXCEPCIONES ---
        kprintf("\nEXCEPCION %d (err=%x) en EIP=%x\n",
                r->int_no, r->err_code, r->eip);
        while (1) {}
    }

    // IRQs (32-47)
    if (r->int_no >= 32 && r->int_no <= 47) {
        unsigned int irq = r->int_no - 32;

        if (irq == 0) {
            // Timer: EOI ANTES de conmutar.
            // Si conmutamos sin EOI, el PIC queda bloqueado y
            // no llegan más IRQs (incluido el teclado).
            pit_tick();
            pic_send_eoi(irq);
            schedule();
            return;
        }

        if (irq == 1) {
            keyboard_handler();
        }
        else if (irq == 12) {
            mouse_handler();
        }

        pic_send_eoi(irq);
    }
}
