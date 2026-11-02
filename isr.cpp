// Copyright (C) 2026 Samuel Tamayo Infante
// (licencia GPL igual que antes...)

#include "isr.h"
#include "screen.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "thread.h"
#include "mouse.h"

// Escribe directamente en VGA sin usar pila ni heap.
// Útil para excepciones fatales (#DF, #PF, #GP) donde kprintf puede fallar.
static void vga_panic(const char *tag, uint32_t code, uint32_t eip, uint32_t cr2) {
    volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
    // Limpiar pantalla
    for (int i = 0; i < 80 * 25; i++) vga[i] = 0x0F20; // espacio blanco
    // Escribir tag (rojo sobre negro)
    int col = 0;
    for (const char *p = tag; *p && col < 80; p++, col++) {
        vga[col] = (0x4F00) | (uint8_t)*p;
    }
    // Hex dump de los 3 valores
    const char *hex = "0123456789ABCDEF";
    int line = 2;
    uint32_t vals[3] = {code, eip, cr2};
    const char *labels[3] = {"code=", "eip =", "cr2 ="};
    for (int v = 0; v < 3; v++) {
        col = 0;
        for (const char *p = labels[v]; *p; p++, col++)
            vga[line * 80 + col] = (0x4F00) | (uint8_t)*p;
        for (int i = 28; i >= 0; i -= 4) {
            vga[line * 80 + col++] = (0x4F00) | hex[(vals[v] >> i) & 0xF];
        }
        line++;
    }
}

extern "C" void isr_handler(Regs *r) {
    // --- Excepciones (0-31) ---
    if (r->int_no < 32) {
        uint32_t cr2 = 0;
        if (r->int_no == 14) {
            asm volatile("mov %%cr2, %0" : "=r"(cr2));
        }

        // Para #DF, #GP, #PF, #SS, #NP, #TS: usar panic VGA (sin pila)
        if (r->int_no == 8 || r->int_no == 13 || r->int_no == 14 ||
            r->int_no == 12 || r->int_no == 11 || r->int_no == 10) {
            const char *tag = "EXCEPCION FATAL";
            switch (r->int_no) {
                case 8:  tag = "DOUBLE FAULT (#DF)"; break;
                case 10: tag = "INVALID TSS (#TS)";  break;
                case 11: tag = "SEGMENT NOT PRESENT (#NP)"; break;
                case 12: tag = "STACK FAULT (#SS)";  break;
                case 13: tag = "GENERAL PROTECTION (#GP)"; break;
                case 14: tag = "PAGE FAULT (#PF)";   break;
            }
            vga_panic(tag, r->err_code, r->eip, cr2);
            while (1) { asm volatile("cli; hlt"); }
        }

        // Resto de excepciones: kprintf (la pila debería estar bien)
        kprintf("\nEXC %d (err=%x) EIP=%x CR2=%x\n",
                r->int_no, r->err_code, r->eip, cr2);
        while (1) { asm volatile("cli; hlt"); }
    }

    // --- IRQs (32-47) ---
    if (r->int_no >= 32 && r->int_no <= 47) {
        unsigned int irq = r->int_no - 32;

        if (irq == 0) {
            pit_tick();
            pic_send_eoi(irq);
            schedule();
            return;
        }
        if (irq == 1) keyboard_handler();
        else if (irq == 12) mouse_handler();

        pic_send_eoi(irq);
    }
}
