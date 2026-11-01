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

#include "mouse.h"
#include "io.h"
#include "pic.h"
#include "screen.h"

// =====================================================================
// Puertos PS/2
// =====================================================================
#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64

#define PS2_STATUS_OUTPUT   0x01   // bit 0: hay dato en 0x60
#define PS2_STATUS_INPUT    0x02   // bit 1: buffer de entrada lleno
#define PS2_STATUS_MOUSE    0x20   // bit 5: el dato viene del ratón

// =====================================================================
// Estado del ratón
// =====================================================================
static int  mouse_ready    = 0;
static int  mouse_x        = 640;
static int  mouse_y        = 400;
static int  mouse_btn_left = 0;
static int  mouse_btn_right= 0;
static int  mouse_btn_mid  = 0;

static uint8_t packet[3];
static int     packet_idx = 0;

// =====================================================================
// Helpers 8042
// =====================================================================

// Espera a que el buffer de entrada del 8042 esté libre (bit1 = 0)
static void kbc_wait_write(void) {
    for (int i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & PS2_STATUS_INPUT) == 0) return;
    }
}

// Espera a que haya un dato listo en el buffer de salida (bit0 = 1)
static void kbc_wait_read(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS) & PS2_STATUS_OUTPUT) return;
    }
}

// Vacía todo lo que haya pendiente en el buffer de salida
static void kbc_flush(void) {
    for (int i = 0; i < 100; i++) {
        if (!(inb(PS2_STATUS) & PS2_STATUS_OUTPUT)) break;
        inb(PS2_DATA);
    }
}

// Escribe un comando al controlador (puerto 0x64)
static void kbc_cmd(uint8_t cmd) {
    kbc_wait_write();
    outb(PS2_CMD, cmd);
}

// Escribe un dato al puerto 0x60
static void kbc_data(uint8_t data) {
    kbc_wait_write();
    outb(PS2_DATA, data);
}

// Envía un byte al RATÓN (prefijo 0xD4) y espera ACK 0xFA
static int mouse_write(uint8_t val) {
    kbc_cmd(0xD4);
    kbc_data(val);
    kbc_wait_read();
    uint8_t ack = inb(PS2_DATA);
    return (ack == 0xFA);
}

// =====================================================================
// Init
// =====================================================================
extern "C" int mouse_init(void) {
    asm volatile("cli");   // proteger el PS/2 durante init

    kprintf("Mouse: inicializando...\n");

    packet_idx = 0;
    mouse_ready = 0;
    mouse_btn_left = 0;
    mouse_btn_right = 0;
    mouse_btn_mid = 0;

    kbc_flush();

    // 1. Habilitar el puerto auxiliar (ratón)
    kbc_cmd(0xA8);

    // 2. Leer el "Compaq status byte" del 8042
    kbc_cmd(0x20);
    kbc_wait_read();
    uint8_t status = inb(PS2_DATA);
    kprintf("Mouse: config = %x\n", status);

    // 3. Activar IRQ1 (teclado) + IRQ12 (ratón), clock ON en ambos
    status |=  0x03;    // bit0 = IRQ1, bit1 = IRQ12
    status &= ~0x30;    // bit4/5 = clock teclado/ratón ON
    kbc_cmd(0x60);
    kbc_data(status);

    // 4. Poner el ratón en modo por defecto
    if (!mouse_write(0xF6)) {
        kprintf("Mouse: no responde a 0xF6\n");
        asm volatile("sti");
        return 0;
    }
    kbc_flush();

    // 5. Habilitar streaming de paquetes
    if (!mouse_write(0xF4)) {
        kprintf("Mouse: no responde a 0xF4\n");
        asm volatile("sti");
        return 0;
    }
    kbc_flush();

    // 6. Desenmascarar IRQ2 (cascade maestro→esclavo) + IRQ12 (ratón)
    //    Sin IRQ2, las IRQs del esclavo (8-15) no llegan a la CPU.
    pic_unmask_irq(2);
    pic_unmask_irq(12);

    mouse_ready = 1;
    kprintf("Mouse: LISTO\n");

    asm volatile("sti");
    return 1;
}

// =====================================================================
// Handler de IRQ12
// =====================================================================
extern "C" void mouse_handler(void) {
    uint8_t status = inb(PS2_STATUS);

    // ¿Hay dato en 0x60?
    if (!(status & PS2_STATUS_OUTPUT)) return;

    // ¿El dato es del ratón?
    // Si no lo es, NO lo leemos: es del teclado y lo debe consumir
    // el handler del teclado. Esto evita "robarle" bytes al teclado
    // y que se desincronice / cuelgue.
    if (!(status & PS2_STATUS_MOUSE)) return;

    uint8_t data = inb(PS2_DATA);

    // El byte 0 del paquete debe tener el bit 3 = 1.
    // Si no, es basura o estamos desincronizados: reiniciar.
    if (packet_idx == 0 && !(data & 0x08)) return;

    packet[packet_idx++] = data;
    if (packet_idx < 3) return;
    packet_idx = 0;

    uint8_t flags = packet[0];

    // Overflow en X o Y: descartar el paquete
    if (flags & 0xC0) return;

    // Botones
    mouse_btn_left  = (flags & 0x01) ? 1 : 0;
    mouse_btn_right = (flags & 0x02) ? 1 : 0;
    mouse_btn_mid   = (flags & 0x04) ? 1 : 0;

    // Desplazamiento X (con extensión de signo si bit4 = 1)
    int dx = (int)packet[1];
    if (flags & 0x10) dx |= 0xFFFFFF00;

    // Desplazamiento Y (con extensión de signo si bit5 = 1)
    int dy = (int)packet[2];
    if (flags & 0x20) dy |= 0xFFFFFF00;
    dy = -dy;   // Y del ratón es hacia arriba; la pantalla es al revés

    mouse_x += dx;
    mouse_y += dy;

    // Sin clamp aquí: los límites los aplica desktop.cpp con
    // gfx_width()/gfx_height() cuando dibuja el cursor.
    // Así no dependemos de gfx_* dentro del handler.
}

// =====================================================================
// Getters
// =====================================================================
extern "C" int mouse_is_ready(void)      { return mouse_ready; }
extern "C" int mouse_get_x(void)         { return mouse_x; }
extern "C" int mouse_get_y(void)         { return mouse_y; }
extern "C" int mouse_left_pressed(void)  { return mouse_btn_left; }
extern "C" int mouse_right_pressed(void) { return mouse_btn_right; }
