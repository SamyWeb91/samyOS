#include "isr.h"
#include "screen.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "thread.h"

extern "C" void isr_handler(Regs *r) {
    if (r->int_no < 32) {
        kprintf("\nEXCEPCION %d (err=%x) en EIP=%x\n",
                r->int_no, r->err_code, r->eip);
        while (1) {}
    } else if (r->int_no >= 32 && r->int_no <= 47) {
        unsigned int irq = r->int_no - 32;

        if (irq == 0) {
            // Timer: solo cuenta ticks.
            // NO llamamos a schedule() aquí para evitar reentrada:
            // si el timer interrumpe mientras un hilo está dentro de
            // schedule()/context_switch, se corrompe el scheduler.
            // La conmutación es cooperativa (thread_yield()).
            pit_tick();
        } else if (irq == 1) {
            keyboard_handler();
        }

        pic_send_eoi(irq);
    }
}
