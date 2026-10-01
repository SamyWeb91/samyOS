#include "isr.h"
#include "screen.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"

extern "C" void isr_handler(Regs *r) {
    if (r->int_no < 32) {
        kprintf("\nEXC %d err=%x EIP=%x\n",
                r->int_no, r->err_code, r->eip);
        while (1) {}
    }
    if (r->int_no >= 32 && r->int_no <= 47) {
        unsigned irq = r->int_no - 32;
        if (irq == 0)      pit_tick();
        else if (irq == 1) keyboard_handler();
        pic_send_eoi(irq);
    }
}
