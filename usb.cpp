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

#include "usb.h"
#include "io.h"
#include "screen.h"
#include "pic.h"

// =====================================================================
// Limitaciones intencionadas:
//  - Un controlador UHCI
//  - Un puerto con un dispositivo (el primero que enumere OK)
//  - Solo HID Boot Protocol (teclado o ratón)
//  - Transferencias por polling (sin IRQ USB)
//  - 32-bit, BAR I/O, identity-mapped
// =====================================================================

// =====================================================================
// PCI config space
// =====================================================================
#define PCI_CFG_ADDR 0xCF8
#define PCI_CFG_DATA 0xCFC

static uint32_t pci_r32(uint8_t b, uint8_t d, uint8_t f, uint8_t off) {
    outl(PCI_CFG_ADDR, (1u << 31) | (b << 16) | (d << 11) | (f << 8) | (off & 0xFC));
    return inl(PCI_CFG_DATA);
}

static uint16_t pci_r16(uint8_t b, uint8_t d, uint8_t f, uint8_t off) {
    uint32_t v = pci_r32(b, d, f, off);
    return (off & 2) ? (uint16_t)(v >> 16) : (uint16_t)(v & 0xFFFF);
}

static void pci_w16(uint8_t b, uint8_t d, uint8_t f, uint8_t off, uint16_t v) {
    outl(PCI_CFG_ADDR, (1u << 31) | (b << 16) | (d << 11) | (f << 8) | (off & 0xFC));
    uint32_t old = inl(PCI_CFG_DATA);
    if (off & 2) old = (old & 0x0000FFFF) | ((uint32_t)v << 16);
    else         old = (old & 0xFFFF0000) | (uint32_t)v;
    outl(PCI_CFG_ADDR, (1u << 31) | (b << 16) | (d << 11) | (f << 8) | (off & 0xFC));
    outl(PCI_CFG_DATA, old);
}

// =====================================================================
// UHCI registros (I/O BAR)
// =====================================================================
#define USBCMD    0x00
#define USBSTS    0x02
#define USBINTR   0x04
#define FRNUM     0x06
#define FLBASEADD 0x08
#define SOFMOD    0x0C
#define PORTSC1   0x10
#define PORTSC2   0x12

// Bits de PORTSC
#define PORTSC_CCS      0x0001   // current connect status
#define PORTSC_CSC      0x0002   // connect status change
#define PORTSC_PED      0x0004   // port enable/disable
#define PORTSC_PEDC     0x0008   // port enable/disable change
#define PORTSC_LSDA     0x0100   // low speed device attached
#define PORTSC_PR       0x0200   // port reset

// Bits de USBSTS
#define USBSTS_HCHALTED 0x0020
#define USBSTS_HCPERR   0x0004

// =====================================================================
// TD / QH (UHCI)
// =====================================================================
struct UhciTD {
    volatile uint32_t link;
    volatile uint32_t cs;
    volatile uint32_t token;
    volatile uint32_t buffer;
} __attribute__((packed, aligned(16)));

struct UhciQH {
    volatile uint32_t head;
    volatile uint32_t element;
} __attribute__((packed, aligned(16)));

// Frame list 1024 entradas
static uint32_t uhci_frames[1024] __attribute__((aligned(4096)));
static UhciQH    uhci_qh        __attribute__((aligned(16)));
static UhciTD    uhci_td[8]     __attribute__((aligned(16)));
static uint8_t   uhci_buf[64]   __attribute__((aligned(4)));

static uint16_t  uhci_iobase = 0;
static int       usb_ready = 0;

// Dispositivo HID
static uint8_t   dev_addr = 0;
static uint8_t   kbd_ep = 0x81;
static uint8_t   mouse_ep = 0x81;
static int       have_kbd = 0;
static int       have_mouse = 0;
static uint8_t   kbd_toggle = 0;
static uint8_t   mouse_toggle = 0;

// Estado ratón / teclado
static int mx = 640, my = 400, mbtn = 0;

#define KBUF 32
static char kbuf[KBUF];
static int  kh = 0, kt = 0;

static void kpush(char c) {
    int n = (kh + 1) % KBUF;
    if (n == kt) return;
    kbuf[kh] = c;
    kh = n;
}

// =====================================================================
// I/O UHCI
// =====================================================================
static inline void uw16(uint16_t off, uint16_t v) { outw(uhci_iobase + off, v); }
static inline uint16_t ur16(uint16_t off) { return inw(uhci_iobase + off); }
static inline void uw32(uint16_t off, uint32_t v) { outl(uhci_iobase + off, v); }
static inline uint32_t ur32(uint16_t off) { return inl(uhci_iobase + off); }

// =====================================================================
// Flags TD
// =====================================================================
#define TD_ACTIVE  0x00800000
#define TD_IOC     0x01000000
#define TD_IOS     0x02000000
#define TD_LS      0x04000000
#define TD_SPD     0x20000000
#define TD_ERRMASK 0x007E0000

#define TOKEN_SETUP 0x2D
#define TOKEN_IN    0x69
#define TOKEN_OUT   0xE1

// =====================================================================
// Espera a que un TD termine
// =====================================================================
static int uhci_wait_td(UhciTD *td, int timeout) {
    while (timeout-- > 0) {
        uint32_t cs = td->cs;
        if (!(cs & TD_ACTIVE)) {
            if (cs & TD_ERRMASK) return 0;
            return 1;
        }
        for (volatile int i = 0; i < 100; i++) {}
    }
    return 0;
}

// =====================================================================
// Control transfer
//   setup8: 8 bytes del setup
//   data  : NULL si no hay data stage
//   len   : longitud del data stage (0 si no hay)
//   data_in: 1 si el data stage es IN, 0 si es OUT
// =====================================================================
static int uhci_control(uint8_t addr, uint8_t *setup8,
                        void *data, int len, int data_in) {
    // Limpiar TDs
    for (int i = 0; i < 8; i++) {
        uhci_td[i].link   = 1;
        uhci_td[i].cs     = 0;
        uhci_td[i].token  = 0;
        uhci_td[i].buffer = 0;
    }

    // SETUP
    uhci_td[0].link   = ((uint32_t)&uhci_td[1]) | 0x4; // depth-first
    uhci_td[0].cs     = TD_ACTIVE | (3 << 27);
    uhci_td[0].token  = (7 << 21) | (0 << 19) | (addr << 8) | TOKEN_SETUP;
    uhci_td[0].buffer = (uint32_t)setup8;

    int idx = 1;
    uint8_t data_toggle = 1;

    if (len > 0 && data) {
        uhci_td[1].link   = ((uint32_t)&uhci_td[2]) | 0x4;
        uhci_td[1].cs     = TD_ACTIVE | (3 << 27);
        uint8_t pid = data_in ? TOKEN_IN : TOKEN_OUT;
        uhci_td[1].token  = ((len - 1) << 21) | (data_toggle << 19) |
                            (addr << 8) | pid;
        uhci_td[1].buffer = (uint32_t)data;
        idx = 2;
        data_toggle ^= 1;
    }

    // STATUS (siempre la dirección contraria al data stage)
    uhci_td[idx].link   = 1;
    uhci_td[idx].cs     = TD_ACTIVE | (3 << 27) | TD_IOC;
    uint8_t st_pid = (len > 0 && data_in) ? TOKEN_OUT : TOKEN_IN;
    uhci_td[idx].token  = (0x7FF << 21) | (1 << 19) | (addr << 8) | st_pid;
    uhci_td[idx].buffer = 0;

    // QH
    uhci_qh.head    = 1;
    uhci_qh.element = (uint32_t)&uhci_td[0];

    uint32_t qhptr = (uint32_t)&uhci_qh | 0x2;
    for (int i = 0; i < 1024; i++)
        uhci_frames[i] = qhptr;

    int ok = 1;
    if (!uhci_wait_td(&uhci_td[0], 100000)) ok = 0;
    if (ok && len > 0 && data) {
        if (!uhci_wait_td(&uhci_td[1], 100000)) ok = 0;
    }
    if (ok) {
        if (!uhci_wait_td(&uhci_td[idx], 100000)) ok = 0;
    }

    // Limpiar schedule
    uhci_qh.element = 1;
    for (int i = 0; i < 1024; i++)
        uhci_frames[i] = 1;

    return ok;
}

static int ctrl_get_desc(uint8_t addr, uint8_t type, uint8_t idx,
                         void *buf, uint8_t len) {
    uint8_t setup[8] = {
        0x80, 0x06, idx, type, 0x00, 0x00, len, 0x00
    };
    return uhci_control(addr, setup, buf, len, 1);
}

static int ctrl_set_addr(uint8_t new_addr) {
    uint8_t setup[8] = {
        0x00, 0x05, new_addr, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    return uhci_control(0, setup, 0, 0, 0);
}

static int ctrl_set_config(uint8_t addr, uint8_t cfg) {
    uint8_t setup[8] = {
        0x00, 0x09, cfg, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    return uhci_control(addr, setup, 0, 0, 0);
}

static int hid_set_protocol(uint8_t addr, uint8_t iface, uint8_t proto) {
    uint8_t setup[8] = {
        0x21, 0x0B, proto, 0x00, iface, 0x00, 0x00, 0x00
    };
    return uhci_control(addr, setup, 0, 0, 0);
}

static int hid_set_idle(uint8_t addr, uint8_t iface) {
    uint8_t setup[8] = {
        0x21, 0x0A, 0x00, 0x00, iface, 0x00, 0x00, 0x00
    };
    return uhci_control(addr, setup, 0, 0, 0);
}

// =====================================================================
// Interrupt IN de un endpoint
//   ep: número de endpoint (1..15). El PID lleva el toggle.
// =====================================================================
static int uhci_interrupt_in(uint8_t addr, uint8_t ep,
                             uint8_t toggle, void *buf, int len) {
    uhci_td[0].link   = 1;
    uhci_td[0].cs     = TD_ACTIVE | (3 << 27);
    uhci_td[0].token  = ((len - 1) << 21) | (toggle << 19) |
                        ((ep & 0x0F) << 15) | (addr << 8) | TOKEN_IN;
    uhci_td[0].buffer = (uint32_t)buf;

    uhci_qh.head    = 1;
    uhci_qh.element = (uint32_t)&uhci_td[0];

    uint32_t qhptr = (uint32_t)&uhci_qh | 0x2;
    for (int i = 0; i < 32; i++)
        uhci_frames[i] = qhptr;

    int ok = uhci_wait_td(&uhci_td[0], 50000);

    uhci_qh.element = 1;
    for (int i = 0; i < 32; i++)
        uhci_frames[i] = 1;
    return ok;
}

// =====================================================================
// HID boot decode
// =====================================================================
static const char hid_boot_to_ascii[128] = {
    0,0,0,0,'a','b','c','d','e','f','g','h','i','j','k','l',
    'm','n','o','p','q','r','s','t','u','v','w','x','y','z',
    '1','2','3','4','5','6','7','8','9','0','\n',27,'\b','\t',' ',
    '-','=','[',']','\\','#',';','\'','`',',','.','/',
};

static void handle_kbd_report(uint8_t *r) {
    static uint8_t prev[6];
    for (int i = 0; i < 6; i++) {
        uint8_t code = r[2 + i];
        if (!code) continue;
        int was = 0;
        for (int j = 0; j < 6; j++) if (prev[j] == code) was = 1;
        if (was) continue;
        if (code < 128) {
            char c = hid_boot_to_ascii[code];
            if (r[0] & 0x22) {  // shift izquierdo o derecho
                if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            }
            if (c) kpush(c);
        }
    }
    for (int i = 0; i < 6; i++) prev[i] = r[2 + i];
}

static void handle_mouse_report(uint8_t *r) {
    int dx = (int8_t)r[1];
    int dy = (int8_t)r[2];
    mbtn = r[0] & 0x07;
    mx += dx;
    my -= dy;
    // Sin clamp aquí: lo hace desktop.cpp con gfx_width/height
}

// =====================================================================
// Reset de puerto
// =====================================================================
static int uhci_port_reset(int port) {
    uint16_t reg = (port == 0) ? PORTSC1 : PORTSC2;
    uint16_t sc = ur16(reg);
    if (!(sc & PORTSC_CCS)) return 0;

    // Reset
    uw16(reg, sc | PORTSC_PR);
    for (volatile int i = 0; i < 500000; i++) {}
    sc = ur16(reg);
    uw16(reg, sc & ~PORTSC_PR);
    for (volatile int i = 0; i < 100000; i++) {}
    sc = ur16(reg);

    // Limpiar CSC si está puesto
    if (sc & PORTSC_CSC) uw16(reg, sc | PORTSC_CSC);

    // Enable
    sc = ur16(reg);
    uw16(reg, sc | PORTSC_PED);
    for (volatile int i = 0; i < 50000; i++) {}
    sc = ur16(reg);
    return (sc & PORTSC_PED) != 0;
}

// =====================================================================
// Enumeración: obtiene descriptor de 8, SET_ADDRESS, descriptor 18,
// SET_CONFIGURATION, encuentra interfaz HID y su endpoint.
// =====================================================================
static int usb_try_enumerate(int port) {
    if (!uhci_port_reset(port)) return 0;

    uint8_t desc[18];
    for (int i = 0; i < 18; i++) desc[i] = 0;

    // 1. GET_DESCRIPTOR device, 8 bytes
    if (!ctrl_get_desc(0, 1, 0, desc, 8)) {
        kprintf("USB: get_desc8 fail port %d\n", port);
        return 0;
    }

    // 2. SET_ADDRESS(1)
    if (!ctrl_set_addr(1)) {
        kprintf("USB: set_addr fail\n");
        return 0;
    }
    dev_addr = 1;
    for (volatile int i = 0; i < 20000; i++) {}

    // 3. GET_DESCRIPTOR device, 18 bytes
    if (!ctrl_get_desc(dev_addr, 1, 0, desc, 18)) {
        kprintf("USB: get_desc18 fail\n");
        return 0;
    }

    kprintf("USB: dev class=%x sub=%x proto=%x vid=%x pid=%x\n",
            desc[4], desc[5], desc[6],
            desc[8] | (desc[9] << 8),
            desc[10] | (desc[11] << 8));

    // 4. GET_DESCRIPTOR config, hasta 64 bytes
    uint8_t cfg[64];
    for (int i = 0; i < 64; i++) cfg[i] = 0;
    if (!ctrl_get_desc(dev_addr, 2, 0, cfg, 64)) {
        kprintf("USB: get_config fail\n");
        return 0;
    }

    int cfg_total = cfg[2] | (cfg[3] << 8);
    if (cfg_total > 64) cfg_total = 64;

    // 5. SET_CONFIGURATION(1)
    if (!ctrl_set_config(dev_addr, 1)) {
        kprintf("USB: set_config fail\n");
        return 0;
    }

    // 6. Parsear descriptores: buscar interfaces HID y sus endpoints
    int found_iface = 0;
    uint8_t iface_proto = 0;
    uint8_t ep_addr = 0;

    int i = cfg[0];  // saltar el config descriptor
    while (i < cfg_total) {
        uint8_t dlen  = cfg[i];
        uint8_t dtype = cfg[i + 1];
        if (dlen == 0) break;

        if (dtype == 0x04) {  // interface descriptor
            iface_proto = cfg[i + 7];   // bInterfaceProtocol
            uint8_t iface_class = cfg[i + 5];
            if (iface_class == 0x03) {  // HID
                found_iface = 1;
                ep_addr = 0;
            } else {
                found_iface = 0;
            }
        } else if (dtype == 0x05 && found_iface) {  // endpoint descriptor
            uint8_t attrs = cfg[i + 3];
            if ((attrs & 0x03) == 0x03) {  // interrupt
                ep_addr = cfg[i + 2];      // bEndpointAddress
            }
        }

        i += dlen;
    }

    if (!ep_addr) {
        kprintf("USB: no encuentro endpoint interrupt\n");
        return 0;
    }

    // 7. Configurar HID en boot protocol
    hid_set_protocol(dev_addr, 0, 0);  // boot
    hid_set_idle(dev_addr, 0);

    // 8. Decidir si es teclado o ratón según el protocolo de la interfaz
    uint8_t ep_num = ep_addr & 0x0F;
    if (iface_proto == 1) {
        have_kbd = 1;
        kbd_ep = ep_num;
        kbd_toggle = 0;
        kprintf("USB: teclado HID en EP%d\n", ep_num);
    } else if (iface_proto == 2) {
        have_mouse = 1;
        mouse_ep = ep_num;
        mouse_toggle = 0;
        kprintf("USB: raton HID en EP%d\n", ep_num);
    } else {
        // Sin protocolo declarado: asumimos ratón si el maxpkt es 4,
        // teclado si es 8. Aproximación.
        have_mouse = 1;
        mouse_ep = ep_num;
        mouse_toggle = 0;
        kprintf("USB: HID generico EP%d, asumimos raton\n", ep_num);
    }

    return 1;
}

// =====================================================================
// API
// =====================================================================
extern "C" int usb_init(void) {
    usb_ready = 0;
    have_kbd = have_mouse = 0;
    dev_addr = 0;

    // Buscar UHCI: class 0x0C subclass 0x03 prog-if 0x00
    for (uint32_t b = 0; b < 256; b++)
    for (uint32_t d = 0; d < 32; d++)
    for (uint32_t f = 0; f < 8; f++) {
        uint32_t id = pci_r32(b, d, f, 0);
        if ((id & 0xFFFF) == 0xFFFF) continue;
        uint32_t cls = pci_r32(b, d, f, 0x08);
        uint8_t prog = (cls >> 8) & 0xFF;
        uint8_t sub  = (cls >> 16) & 0xFF;
        uint8_t base = (cls >> 24) & 0xFF;
        if (base != 0x0C || sub != 0x03 || prog != 0x00)
            continue;

        uint32_t bar4 = pci_r32(b, d, f, 0x20);
        if (!(bar4 & 1)) continue;
        uint32_t bar = bar4 & ~0x3;
        if (bar > 0xFFFF) {
            kprintf("USB: BAR UHCI en 0x%x > 0xFFFF, no soportado\n", bar);
            continue;
        }
        uhci_iobase = (uint16_t)bar;

        // Bus master + IO
        uint16_t cmd = pci_r16(b, d, f, 0x04);
        cmd |= 0x05;
        pci_w16(b, d, f, 0x04, cmd);

        kprintf("USB: UHCI en PCI %x:%x.%x IO=%x\n", b, d, f, uhci_iobase);

        // Reset host controller
        uw16(USBCMD, 0x0004);  // HCRESET
        for (volatile int i = 0; i < 100000; i++) {}
        uw16(USBCMD, 0);       // clear
        uw16(USBINTR, 0);
        uw16(USBSTS, 0xFFFF);  // limpiar status

        // Frame list inicial: todas terminate
        for (int i = 0; i < 1024; i++)
            uhci_frames[i] = 1;

        uw32(FLBASEADD, (uint32_t)uhci_frames);
        uw16(FRNUM, 0);
        uw16(SOFMOD, 64);
        uw16(USBCMD, 0x0001);  // Run

        // Enumerar en cada puerto
        for (int p = 0; p < 2; p++) {
            if (usb_try_enumerate(p)) {
                usb_ready = 1;
                kprintf("USB: dispositivo listo\n");
                return 1;
            }
        }
        kprintf("USB: UHCI sin dispositivo HID\n");
        return 0;
    }
    kprintf("USB: no hay UHCI\n");
    return 0;
}

extern "C" int usb_is_ready(void)     { return usb_ready; }
extern "C" int usb_have_kbd(void)     { return have_kbd; }
extern "C" int usb_have_mouse(void)   { return have_mouse; }

extern "C" void usb_poll(void) {
    if (!usb_ready) return;
    uint8_t report[8];

    if (have_kbd) {
        for (int i = 0; i < 8; i++) report[i] = 0;
        if (uhci_interrupt_in(dev_addr, kbd_ep, kbd_toggle, report, 8)) {
            handle_kbd_report(report);
            kbd_toggle ^= 1;
        }
    }
    if (have_mouse) {
        for (int i = 0; i < 4; i++) report[i] = 0;
        if (uhci_interrupt_in(dev_addr, mouse_ep, mouse_toggle, report, 4)) {
            handle_mouse_report(report);
            mouse_toggle ^= 1;
        }
    }
}

extern "C" int usb_mouse_x(void)       { return mx; }
extern "C" int usb_mouse_y(void)       { return my; }
extern "C" int usb_mouse_buttons(void) { return mbtn; }

extern "C" char usb_keyboard_getchar(void) {
    if (kh == kt) return 0;
    char c = kbuf[kt];
    kt = (kt + 1) % KBUF;
    return c;
}
