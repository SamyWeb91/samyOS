// usb_xhci.cpp
#include <stdint.h>
#include "usb.h"
#include "screen.h"
#include "io.h"
#include "paging.h"

/* =====================================================================
 * PCI
 * ===================================================================== */
#define PCI_CFG_ADDR 0xCF8
#define PCI_CFG_DATA 0xCFC

static uint32_t pci32(uint8_t b, uint8_t d, uint8_t f, uint8_t o) {
    outl(PCI_CFG_ADDR, (1U<<31)|((uint32_t)b<<16)|((uint32_t)d<<11)|((uint32_t)f<<8)|(o&0xFC));
    return inl(PCI_CFG_DATA);
}
static uint16_t pci16(uint8_t b, uint8_t d, uint8_t f, uint8_t o) {
    uint32_t v = pci32(b,d,f,o);
    return (o&2) ? (uint16_t)(v>>16) : (uint16_t)(v&0xFFFF);
}
static void pci16w(uint8_t b, uint8_t d, uint8_t f, uint8_t o, uint16_t val) {
    outl(PCI_CFG_ADDR, (1U<<31)|((uint32_t)b<<16)|((uint32_t)d<<11)|((uint32_t)f<<8)|(o&0xFC));
    uint32_t old = inl(PCI_CFG_DATA);
    if (o&2) old = (old&0xFFFF) | ((uint32_t)val<<16);
    else     old = (old&0xFFFF0000) | val;
    outl(PCI_CFG_ADDR, (1U<<31)|((uint32_t)b<<16)|((uint32_t)d<<11)|((uint32_t)f<<8)|(o&0xFC));
    outl(PCI_CFG_DATA, old);
}

/* =====================================================================
 * Registros xHCI
 * ===================================================================== */
struct xhci_cap {
    volatile uint32_t caplen_ver;
    volatile uint32_t hcsparams1;
    volatile uint32_t hcsparams2;
    volatile uint32_t hcsparams3;
    volatile uint32_t hccparams1;
    volatile uint32_t dboff;
    volatile uint32_t rtsoff;
    volatile uint32_t hccparams2;
} __attribute__((packed));

struct xhci_op {
    volatile uint32_t usbcmd;
    volatile uint32_t usbsts;
    volatile uint32_t pagesize;
    volatile uint32_t pad1[2];
    volatile uint32_t dnctrl;
    volatile uint32_t crcr_lo;
    volatile uint32_t crcr_hi;
    volatile uint32_t pad2[4];
    volatile uint32_t dcbaap_lo;
    volatile uint32_t dcbaap_hi;
    volatile uint32_t config;
} __attribute__((packed));

struct xhci_port {
    volatile uint32_t portsc;
    volatile uint32_t portpmsc;
    volatile uint32_t portli;
    volatile uint32_t res;
} __attribute__((packed));

struct xhci_interrupter {
    volatile uint32_t iman;
    volatile uint32_t imod;
    volatile uint32_t erstsz;
    volatile uint32_t pad;
    volatile uint64_t erstba;
    volatile uint64_t erdp;
} __attribute__((packed));

struct xhci_rt {
    volatile uint32_t mfindex;
    volatile uint32_t pad[7];
    struct xhci_interrupter irs[1];
} __attribute__((packed));

struct xhci_trb {
    uint64_t param;
    uint32_t status;
    uint32_t control;
} __attribute__((packed));

/* TRB types */
#define TRB_NORMAL      1
#define TRB_SETUP       2
#define TRB_DATA        3
#define TRB_STATUS      4
#define TRB_LINK        6
#define TRB_ENABLE_SLOT 9
#define TRB_ADDRESS_DEV 11
#define TRB_CONFIG_EP   12

/* Completion codes */
#define CC_SUCCESS      1
#define CC_SHORT_PACKET 13

/* =====================================================================
 * Estado global
 * ===================================================================== */
struct xhci_cap *xhci_cap = 0;
struct xhci_op  *xhci_op  = 0;
struct xhci_rt  *xhci_rt  = 0;
volatile uint32_t *xhci_db = 0;

struct xhci_trb *cr_ring = 0;
uint32_t cr_cycle = 1;
int      cr_enq   = 0;

struct xhci_trb *er_ring = 0;
uint32_t er_cycle = 1;
int      er_deq   = 0;

uint64_t *dcbaa = 0;
struct xhci_trb *er_seg = 0;
uint64_t *er_erst = 0;

int xhci_ok = 0;
int xhci_max_slots = 0;
int xhci_max_ports = 0;

/* =====================================================================
 * Wait event
 * ===================================================================== */
extern "C" int xhci_wait_event(uint32_t *type, uint64_t *param,
                               uint32_t *status, int timeout) {
    while (timeout-- > 0) {
        struct xhci_trb *t = &er_ring[er_deq];
        if ((t->control & 1) == er_cycle) {
            if (type)   *type   = (t->control >> 10) & 0x3F;
            if (param)  *param  = t->param;
            if (status) *status = (t->status >> 24) & 0xFF;

            xhci_rt->irs[0].erdp = (uint64_t)(uintptr_t)&er_ring[er_deq];
            er_deq++;
            if (er_deq == 256) { er_deq = 0; er_cycle ^= 1; }
            return 1;
        }
        __asm__ __volatile__("pause");
    }
    return 0;
}

extern "C" void xhci_ring_doorbell(int slot, int target) {
    xhci_db[slot] = target;
}

/* =====================================================================
 * Enviar comando
 * ===================================================================== */
extern "C" int xhci_send_cmd(uint32_t p1, uint32_t p2, uint32_t st, uint32_t ctrl,
                             uint32_t *out_type, uint32_t *out_status, uint64_t *out_param) {
    int idx = cr_enq;
    cr_ring[idx].param   = ((uint64_t)p2 << 32) | p1;
    cr_ring[idx].status  = st;
    cr_ring[idx].control = (ctrl & ~1) | cr_cycle;

    cr_enq++;
    if (cr_enq == 255) {
        cr_ring[255].param   = (uint64_t)(uintptr_t)cr_ring;
        cr_ring[255].status  = 0;
        cr_ring[255].control = (TRB_LINK << 10) | (1 << 1) | (cr_cycle ^ 1);
        cr_enq = 0;
        cr_cycle ^= 1;
    }

    xhci_db[0] = 0;

    uint32_t t, s;
    uint64_t p;
    if (!xhci_wait_event(&t, &p, &s, 10000000)) return -1;
    if (out_type)   *out_type   = t;
    if (out_status) *out_status = s;
    if (out_param)  *out_param  = p;
    return 0;
}

/* =====================================================================
 * Init
 * ===================================================================== */
extern "C" int xhci_init(void) {
    xhci_ok = 0;
    usb_log_add("xhci: init");

    /* 1. PCI scan */
    int fb = -1, fd = -1, ff = -1;
    for (int b = 0; b < 256 && fb < 0; b++)
    for (int d = 0; d < 32 && fb < 0; d++)
    for (int f = 0; f < 8; f++) {
        uint32_t id = pci32(b, d, f, 0x00);
        if ((id & 0xFFFF) == 0xFFFF) continue;

        uint32_t cr = pci32(b, d, f, 0x08);
        uint8_t cls  = (cr >> 24) & 0xFF;
        uint8_t sub  = (cr >> 16) & 0xFF;
        uint8_t prog = (cr >> 8)  & 0xFF;

        if (cls == 0x0C && sub == 0x03 && prog == 0x30) {
            fb = b; fd = d; ff = f;
            break;
        }
    }

    if (fb < 0) { usb_log_add("xhci: no PCI"); return 0; }
    usb_log_add("xhci: controlador en PCI");

    /* 2. Enable MMIO + Bus Master */
    uint16_t cmd = pci16(fb, fd, ff, 0x04);
    cmd |= (1 << 1) | (1 << 2);
    pci16w(fb, fd, ff, 0x04, cmd);

    /* 3. Leer BAR0 (64-bit) */
    uint32_t bar0 = pci32(fb, fd, ff, 0x10) & 0xFFFFFFF0;
    uint32_t bar1 = pci32(fb, fd, ff, 0x14);
    uint64_t phys = ((uint64_t)bar1 << 32) | bar0;

    if (phys == 0) { usb_log_add("xhci: BAR0=0"); return 0; }

    paging_map_range((uint32_t)phys, (uint32_t)phys, 16,
                     PTE_PRESENT | PTE_RW | PTE_PCD | PTE_PWT);

    xhci_cap = (struct xhci_cap *)(uintptr_t)phys;
    uint8_t caplen = xhci_cap->caplen_ver & 0xFF;
    uint32_t rtsoff = xhci_cap->rtsoff & ~0x1F;
    uint32_t dboff  = xhci_cap->dboff  & ~0x03;

    xhci_op = (struct xhci_op *)((uintptr_t)phys + caplen);
    xhci_rt = (struct xhci_rt *)((uintptr_t)phys + rtsoff);
    xhci_db = (volatile uint32_t *)((uintptr_t)phys + dboff);

    usb_log_u32("xhci: caplen=", caplen);

    /* 4. Reset */
    xhci_op->usbcmd &= ~1;
    for (int i = 0; i < 1000000; i++) {
        if (xhci_op->usbsts & 1) break;
        __asm__ __volatile__("pause");
    }

    xhci_op->usbcmd |= (1 << 1);
    for (int i = 0; i < 1000000; i++) {
        if (!(xhci_op->usbcmd & (1 << 1))) break;
        __asm__ __volatile__("pause");
    }
    for (int i = 0; i < 1000000; i++) {
        if (!(xhci_op->usbsts & (1 << 11))) break;
        __asm__ __volatile__("pause");
    }

    usb_log_add("xhci: reset ok");

    /* 5. MaxSlots + MaxPorts */
    xhci_max_slots = xhci_cap->hcsparams1 & 0xFF;
    xhci_max_ports = (xhci_cap->hcsparams1 >> 24) & 0xFF;
    usb_log_u32("xhci: slots=", xhci_max_slots);
    usb_log_u32("xhci: ports=", xhci_max_ports);

    xhci_op->config = xhci_max_slots;

    /* 6. DCBAA (array de 256 entradas de 8 bytes = 2 KB) */
    static uint64_t dcbaa_mem[256] __attribute__((aligned(64)));
    dcbaa = dcbaa_mem;
    for (int i = 0; i < 256; i++) dcbaa[i] = 0;
    xhci_op->dcbaap_lo = (uint32_t)(uintptr_t)dcbaa;
    xhci_op->dcbaap_hi = 0;   /* en 32 bits, high dword siempre 0 */

    /* 7. Event Ring (256 TRBs de 16 bytes) */
    static struct xhci_trb er_mem[256] __attribute__((aligned(64)));
    er_ring = er_mem;
    for (int i = 0; i < 256; i++) {
        er_mem[i].param = 0; er_mem[i].status = 0; er_mem[i].control = 0;
    }

    static uint64_t erst_mem[2] __attribute__((aligned(64)));
    er_erst = erst_mem;
    erst_mem[0] = (uint64_t)(uintptr_t)er_mem;
    erst_mem[1] = 256;

    xhci_rt->irs[0].erstsz = 1;
    xhci_rt->irs[0].erdp   = (uint64_t)(uintptr_t)er_mem;
    xhci_rt->irs[0].erstba = (uint64_t)(uintptr_t)erst_mem;
    xhci_rt->irs[0].iman   = 2;

    /* 8. Command Ring (256 TRBs) */
    static struct xhci_trb cr_mem[256] __attribute__((aligned(64)));
    cr_ring = cr_mem;
    for (int i = 0; i < 256; i++) {
        cr_mem[i].param = 0; cr_mem[i].status = 0; cr_mem[i].control = 0;
    }
    cr_mem[255].param   = (uint64_t)(uintptr_t)cr_mem;
    cr_mem[255].control = (TRB_LINK << 10) | (1 << 1) | 1;

    xhci_op->crcr_lo = ((uint32_t)(uintptr_t)cr_mem) | 1;
    xhci_op->crcr_hi = 0;   /* en 32 bits, high dword siempre 0 */

    /* 9. Arrancar */
    xhci_op->usbcmd |= (1 << 0) | (1 << 2);

    usb_log_add("xhci: arrancado");
    xhci_ok = 1;
    return 1;
}

/* =====================================================================
 * Helpers expuestos a usb_dev.cpp
 * ===================================================================== */
extern "C" void *xhci_get_op_regs(void) { return (void*)xhci_op; }
extern "C" int   xhci_get_max_ports(void) { return xhci_max_ports; }

extern "C" void *xhci_get_dcbaa(void) { return (void*)dcbaa; }
