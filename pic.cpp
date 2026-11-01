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

#include "pic.h"
#include "io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

// =====================================================================
// Remapear el PIC: IRQ0-7 -> INT 0x20-0x27, IRQ8-15 -> INT 0x28-0x2F
// Al final, solo IRQ0 (timer) e IRQ1 (teclado) quedan desenmascaradas.
// =====================================================================
void pic_remap() {
    // Iniciar secuencia de inicialización
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();

    // Offsets de los vectores
    outb(PIC1_DATA, 0x20); io_wait();  // IRQ0-7  -> INT 0x20-0x27
    outb(PIC2_DATA, 0x28); io_wait();  // IRQ8-15 -> INT 0x28-0x2F

    // Conexión maestro-esclavo
    outb(PIC1_DATA, 0x04); io_wait();  // IRQ2 del maestro conecta con el esclavo
    outb(PIC2_DATA, 0x02); io_wait();  // Identidad del esclavo

    // Modo 8086
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    // --- Máscaras: 0 = desenmascarado, 1 = enmascarado ---
    // PIC1: solo IRQ0 (timer) e IRQ1 (teclado) desenmascarados.
    // 0xFC = 1111 1100 -> IRQ0=0, IRQ1=0, IRQ2..7=1
    outb(PIC1_DATA, 0xFC);
    // PIC2: todo enmascarado.
    outb(PIC2_DATA, 0xFF);
}

// =====================================================================
// Enviar EOI (End Of Interrupt) al PIC
// =====================================================================
void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}

// =====================================================================
// Enmascarar una IRQ (0-15)
// =====================================================================
void pic_mask_irq(unsigned char irq) {
    unsigned short port;
    unsigned char value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) | (1 << irq);
    outb(port, value);
}

// =====================================================================
// Desenmascarar una IRQ (0-15)
// =====================================================================
void pic_unmask_irq(unsigned char irq) {
    unsigned short port;
    unsigned char value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}
