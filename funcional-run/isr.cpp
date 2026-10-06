#include "isr.h"
#include "screen.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "thread.h"

extern "C" void isr_handler(Regs *r) {
    // Excepciones (0-31)
    if (r->int_no < 32) {
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

        pic_send_eoi(irq);
    }
}
