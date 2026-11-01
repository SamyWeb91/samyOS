// Copyright (C) 2026 Samuel Tamayo Infante
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "isr.h"
#include "screen.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "thread.h"
#include "mouse.h"

extern "C" void isr_handler(Regs *r) {
    // Excepciones (0-31)
    if (r->int_no < 32) {
        unsigned int cr2 = 0;
        // Si es un Page Fault (14), leer CR2 para saber qué dirección falló
        if (r->int_no == 14) {
            asm volatile("mov %%cr2, %0" : "=r"(cr2));
        }

        kprintf("\nEXC %d (err=%x) EIP=%x CR2=%x\n",
                r->int_no, r->err_code, r->eip, cr2);
        
        // Bucle infinito seguro: cli (desactiva interrupciones) + hlt (duerme la CPU)
        while (1) {
            asm volatile("cli; hlt");
        }
    }

    // IRQs (32-47)
    if (r->int_no >= 32 && r->int_no <= 47) {
        unsigned int irq = r->int_no - 32;

        if (irq == 0) {
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
