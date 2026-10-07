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

#define PS2_STATUS_OUTPUT   0x01
#define PS2_STATUS_INPUT    0x02

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
// Helpers
// =====================================================================
static void ps2_wait_write(void) {
    int t = 100000;
    while ((inb(PS2_STATUS) & PS2_STATUS_INPUT) && --t) {}
}

static int ps2_wait_read(void) {
    int t = 100000;
    while (!(inb(PS2_STATUS) & PS2_STATUS_OUTPUT) && --t) {}
    return t > 0;
}

static void ps2_send_cmd(uint8_t cmd) {
    ps2_wait_write();
    outb(PS2_CMD, cmd);
}

static void ps2_send_data(uint8_t data) {
    ps2_wait_write();
    outb(PS2_DATA, data);
}

static uint8_t ps2_read_data(void) {
    if (!ps2_wait_read()) return 0xFF;
    return inb(PS2_DATA);
}

static int mouse_send_cmd(uint8_t cmd) {
    ps2_send_cmd(0xD4);
    ps2_send_data(cmd);

    int t = 10000;
    while (!(inb(PS2_STATUS) & PS2_STATUS_OUTPUT) && --t) {}
    if (t == 0) return 0;

    uint8_t ack = inb(PS2_DATA);
    return (ack == 0xFA) ? 1 : 0;
}

// =====================================================================
// Init
// =====================================================================
extern "C" int mouse_init(void) {
    asm volatile("cli");

    kprintf("Mouse: inicializando...\n");

    // 1. Habilitar el puerto auxiliar
    ps2_send_cmd(0xA8);

    // 2. Leer config del 8042
    ps2_send_cmd(0x20);
    uint8_t config = ps2_read_data();
    kprintf("Mouse: config = %x\n", config);

    // 3. Modificar config: IRQ1 + IRQ12, clock ON
    config |= 0x03;
    config &= ~0x30;
    ps2_send_cmd(0x60);
    ps2_send_data(config);

    // 4. Reset del ratón
    kprintf("Mouse: reset...\n");
    if (!mouse_send_cmd(0xFF)) {
        kprintf("Mouse: no responde al reset\n");
        asm volatile("sti");
        return 0;
    }

    // 5. Vaciar TODO lo que el reset deje pendiente
    //    (0xFA ya lo leyó mouse_send_cmd; quedan 0xAA y 0x00)
    int t = 100000;
    while ((inb(PS2_STATUS) & PS2_STATUS_OUTPUT) && --t) {
        inb(PS2_DATA);
    }

    // ==========================================================
    // ETAPA 1: NO habilitar reporting (0xF4).
    // Si se envía 0xF4 sin IRQ12 desenmascarada + handler,
    // el ratón llena el buffer del 8042 y el teclado se cuelga.
    // ==========================================================
     kprintf("Mouse: enable reporting...\n");
     if (!mouse_send_cmd(0xF4)) {
         kprintf("Mouse: no responde al enable\n");
         asm volatile("sti");
         return 0;
     }

     pic_unmask_irq(12);   // ← sigue comentado en Etapa 1

    mouse_ready = 1;
    kprintf("Mouse: LISTO (sin reporting)\n");

    asm volatile("sti");
    return 1;
}

// =====================================================================
// Handler de IRQ12 (no se usa en Etapa 1, pero queda listo)
// =====================================================================
extern "C" void mouse_handler(void) {
    uint8_t data = inb(PS2_DATA);

    if (packet_idx == 0 && !(data & 0x08)) return;

    packet[packet_idx++] = data;
    if (packet_idx < 3) return;
    packet_idx = 0;

    uint8_t flags = packet[0];
    if (!(flags & 0x08)) return;
    if (flags & 0xC0) return;

    mouse_btn_left  = (flags & 0x01) ? 1 : 0;
    mouse_btn_right = (flags & 0x02) ? 1 : 0;
    mouse_btn_mid   = (flags & 0x04) ? 1 : 0;

    int dx = (int)packet[1];
    if (flags & 0x10) dx |= 0xFFFFFF00;

    int dy = (int)packet[2];
    if (flags & 0x20) dy |= 0xFFFFFF00;
    dy = -dy;

    mouse_x += dx;
    mouse_y += dy;
}

// =====================================================================
// Getters
// =====================================================================
extern "C" int mouse_is_ready(void)     { return mouse_ready; }
extern "C" int mouse_get_x(void)        { return mouse_x; }
extern "C" int mouse_get_y(void)        { return mouse_y; }
extern "C" int mouse_left_pressed(void) { return mouse_btn_left; }
extern "C" int mouse_right_pressed(void){ return mouse_btn_right; }
