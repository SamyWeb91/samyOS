// usb_dev.cpp
#include <stdint.h>
#include "usb.h"
#include "screen.h"

/* =====================================================================
 * Contextos xHCI (los definimos aqui porque son distintos de los de
 * usb_xhci.cpp, que solo tiene los registros del controlador).
 * ===================================================================== */
struct xhci_slot_ctx {
    uint32_t dw0;
    uint32_t dw1;
    uint32_t dw2;
    uint32_t dw3;
} __attribute__((packed));

struct xhci_ep_ctx {
    uint32_t dw0;
    uint32_t dw1;
    uint32_t dw2;
    uint32_t dw3;
    uint32_t dw4;
} __attribute__((packed));

struct xhci_input_ctrl_ctx {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t pad[6];
} __attribute__((packed));

struct xhci_input_ctx {
    struct xhci_input_ctrl_ctx ctrl;
    struct xhci_slot_ctx slot;
    struct xhci_ep_ctx eps[31];
} __attribute__((packed));

/* TRB: misma estructura que en usb_xhci.cpp. Como no se pueden
 * compartir cabeceras facilmente en este kernel, la redeclaramos aqui
 * con el mismo layout. */
struct xhci_trb {
    uint64_t param;
    uint32_t status;
    uint32_t control;
} __attribute__((packed));

/* Buffers estaticos */
static struct xhci_input_ctx ic_global __attribute__((aligned(64)));
static uint8_t dev_ctx_mem[2048] __attribute__((aligned(64)));
static uint8_t ep0_ring_mem[4096] __attribute__((aligned(64)));

/* Endpoint 0 ring */
static struct xhci_trb *ep0_ring = 0;
static uint32_t ep0_cycle = 1;
static int      ep0_enq = 0;

/* Slots activos */
#define MAX_SLOTS 32
static int slot_active[MAX_SLOTS] = {0};

/* =====================================================================
 * Helpers locales
 * ===================================================================== */
static void memzero(void *p, uint32_t n) {
    uint8_t *b = (uint8_t *)p;
    for (uint32_t i = 0; i < n; i++) b[i] = 0;
}

/* =====================================================================
 * Setup del ring del endpoint 0
 * ===================================================================== */
static int setup_ep0_ring(int slot) {
    ep0_ring = (struct xhci_trb *)ep0_ring_mem;
    for (int i = 0; i < 256; i++) {
        ep0_ring[i].param = 0;
        ep0_ring[i].status = 0;
        ep0_ring[i].control = 0;
    }
    ep0_ring[255].param   = (uint64_t)(uintptr_t)ep0_ring_mem;
    ep0_ring[255].control = (6 << 10) | (1 << 1) | 1;   /* TRB_LINK */
    ep0_cycle = 1;
    ep0_enq = 0;
    (void)slot;
    return 0;
}

/* =====================================================================
 * Add TRB al ring del endpoint 0
 * ===================================================================== */
static int ep0_add(uint64_t param, uint32_t status, uint32_t control) {
    int idx = ep0_enq;
    ep0_ring[idx].param   = param;
    ep0_ring[idx].status  = status;
    ep0_ring[idx].control = (control & ~1) | ep0_cycle;

    ep0_enq++;
    if (ep0_enq == 255) {
        ep0_ring[255].param   = (uint64_t)(uintptr_t)ep0_ring_mem;
        ep0_ring[255].control = (6 << 10) | (1 << 1) | (ep0_cycle ^ 1);
        ep0_enq = 0;
        ep0_cycle ^= 1;
    }
    return 0;
}

/* =====================================================================
 * Control transfer por el endpoint 0
 * ===================================================================== */
static int usb_control(int slot, uint8_t bmReq, uint8_t bReq,
                       uint16_t wValue, uint16_t wIndex,
                       void *data, uint16_t wLength, int is_in) {
    if (slot < 0 || slot >= MAX_SLOTS) return -1;
    if (!slot_active[slot]) return -1;

    uint8_t setup[8];
    setup[0] = bmReq;
    setup[1] = bReq;
    setup[2] = wValue & 0xFF;
    setup[3] = (wValue >> 8) & 0xFF;
    setup[4] = wIndex & 0xFF;
    setup[5] = (wIndex >> 8) & 0xFF;
    setup[6] = wLength & 0xFF;
    setup[7] = (wLength >> 8) & 0xFF;

    uint64_t setup_param = 0;
    for (int i = 0; i < 8; i++) setup_param |= ((uint64_t)setup[i]) << (i * 8);

    uint32_t setup_ctrl = (2 << 10);        /* TRB_SETUP */
    if (wLength == 0)  setup_ctrl |= (0 << 16);   /* TRT = No data */
    else if (is_in)    setup_ctrl |= (3 << 16);   /* TRT = IN */
    else               setup_ctrl |= (2 << 16);   /* TRT = OUT */
    setup_ctrl |= (1 << 6);                 /* IDT */
    setup_ctrl |= (8 << 17);                /* Transfer Length = 8 */

    ep0_add(setup_param, 0, setup_ctrl);

    if (wLength > 0 && data) {
        uint32_t data_ctrl = (3 << 10);     /* TRB_DATA */
        if (is_in) data_ctrl |= (1 << 16);  /* DIR: IN */
        data_ctrl |= ((uint32_t)wLength << 17);
        ep0_add((uint64_t)(uintptr_t)data, 0, data_ctrl);
    }

    uint32_t stat_ctrl = (4 << 10);         /* TRB_STATUS */
    if (is_in) stat_ctrl |= (0 << 16);      /* OUT */
    else       stat_ctrl |= (1 << 16);      /* IN */
    stat_ctrl |= (1 << 5);                  /* IOC */
    ep0_add(0, 0, stat_ctrl);

    xhci_ring_doorbell(slot, 1);

    uint32_t type, status;
    uint64_t param;
    if (!xhci_wait_event(&type, &param, &status, 5000000)) return -1;
    if (type != 32) return -1;                    /* Transfer Event */
    if (status != 1 && status != 13) return -1;   /* Success o Short Packet */

    return 0;
}

/* =====================================================================
 * GET_DESCRIPTOR
 * ===================================================================== */
extern "C" int usb_get_descriptor(int slot, int type, int index, void *buf, int len) {
    return usb_control(slot, 0x80, 0x06, (uint16_t)((type << 8) | index),
                       0, buf, (uint16_t)len, 1);
}

/* =====================================================================
 * SET_CONFIGURATION
 * ===================================================================== */
extern "C" int usb_set_config(int slot, int cfg) {
    return usb_control(slot, 0x00, 0x09, (uint16_t)cfg, 0, 0, 0, 0);
}

/* =====================================================================
 * ADDRESS DEVICE
 * ===================================================================== */
extern "C" int usb_addr_device(int slot, int port) {
    memzero(&ic_global, sizeof(ic_global));

    /* Add flags: Slot (bit 0) + Endpoint 0 (bit 1) */
    ic_global.ctrl.add_flags = 3;

    /* Slot Context: Speed=Full (1), Port = port */
    ic_global.slot.dw0 = (1 << 27) | ((uint32_t)port << 16);
    ic_global.slot.dw1 = 0;

    /* Endpoint 0 Context */
    ic_global.eps[0].dw0 = 0;
    ic_global.eps[0].dw1 = (3 << 1) | (4 << 3) | (8 << 16);  /* CErr=3, Type=Control, MaxPkt=8 */
    ic_global.eps[0].dw2 = (uint32_t)(uintptr_t)ep0_ring_mem | 1;  /* DCS=1 */
    ic_global.eps[0].dw3 = 0;
    ic_global.eps[0].dw4 = 0;

    /* Poner el slot context en el DCBAA del slot */
    /* dcbaa es global de usb_xhci.cpp. En lugar de exponerlo, usamos
       una funcion auxiliar que devuelve su direccion. */
    extern void *xhci_get_dcbaa(void);
    uint64_t *dcbaa = (uint64_t *)xhci_get_dcbaa();
    if (dcbaa) dcbaa[slot] = (uint64_t)(uintptr_t)dev_ctx_mem;

    /* Comando Address Device */
    uint32_t st;
    if (xhci_send_cmd((uint32_t)(uintptr_t)&ic_global, 0,
                      0, (11 << 10) | ((uint32_t)slot << 24), 0, &st, 0) != 0) {
        return -1;
    }
    if (st != 1) return -1;

    slot_active[slot] = 1;
    return 0;
}

/* =====================================================================
 * Enumerar dispositivos
 * ===================================================================== */
extern "C" int usb_enumerate_devices(void) {
    usb_log_add("usb: enumerando");

    volatile uint32_t *op_regs = (volatile uint32_t *)xhci_get_op_regs();
    int max_ports = xhci_get_max_ports();

    for (int port = 1; port <= max_ports; port++) {
        volatile uint32_t *portsc = (volatile uint32_t *)
            ((uintptr_t)op_regs + 0x400 + port * 0x10);
        uint32_t sc = *portsc;

        if (!(sc & 1)) continue;    /* No hay dispositivo conectado */

        usb_log_u32("usb: dispositivo en puerto ", port);

        /* Reset del puerto */
        *portsc = (sc & 0x0E00C3E0) | (1 << 4);
        for (int i = 0; i < 1000000; i++) {
            if (!(*portsc & (1 << 4))) break;
            __asm__ __volatile__("pause");
        }

        /* Enable Slot */
        uint32_t type, status;
        uint64_t param;
        if (xhci_send_cmd(0, 0, 0, (9 << 10), &type, &status, &param) != 0)
            continue;
        if (type != 33 || status != 1)
            continue;

        int slot = (int)((param >> 24) & 0xFF);
        usb_log_u32("usb: slot asignado ", (uint32_t)slot);

        /* Preparar ring del endpoint 0 */
        setup_ep0_ring(slot);

        /* Address Device */
        if (usb_addr_device(slot, port) != 0) {
            usb_log_add("usb: address device fallo");
            continue;
        }

        /* GET_DESCRIPTOR (Device, 18 bytes) */
        uint8_t desc[18];
        if (usb_get_descriptor(slot, 1, 0, desc, 18) != 0) {
            usb_log_add("usb: get descriptor fallo");
            continue;
        }

        usb_log_u32("usb: vendor=", (uint32_t)(desc[8] | (desc[9] << 8)));
        usb_log_u32("usb: product=", (uint32_t)(desc[10] | (desc[11] << 8)));
        usb_log_u32("usb: class=", (uint32_t)desc[4]);

        /* SET_CONFIGURATION(1) */
        if (usb_set_config(slot, 1) != 0) {
            usb_log_add("usb: set config fallo");
            continue;
        }

        /* Solo nos interesa Mass Storage (class 8) */
        if (desc[4] == 0x08) {
            usb_log_add("usb: Mass Storage detectado");
            return slot;
        }
    }

    usb_log_add("usb: nada que leer");
    return -1;
}
