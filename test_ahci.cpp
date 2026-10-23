// test_ahci.cpp
// Test de detección AHCI (SOLO LECTURA) para hardware real.
// Basado en ahci.cpp del proyecto MINIOS.
//
// Uso desde el kernel:
//     extern "C" void test_ahci_run(void);
//     test_ahci_run();

#include <stdint.h>
#include "screen.h"
#include "io.h"
#include "paging.h"

/* =====================================================================
 * PCI
 * ===================================================================== */
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

#define PCI_CLASS_STORAGE   0x01
#define PCI_SUBCLASS_AHCI   0x06
#define PCI_PROGIF_AHCI     0x01

static uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off)
{
    uint32_t address =
        (1U << 31) |
        ((uint32_t)bus  << 16) |
        ((uint32_t)dev  << 11) |
        ((uint32_t)func << 8)  |
        (off & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static uint16_t pci_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off)
{
    uint32_t v = pci_read32(bus, dev, func, off);
    if (off & 2) return (uint16_t)(v >> 16);
    return (uint16_t)(v & 0xFFFF);
}

static void pci_write16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off, uint16_t val)
{
    uint32_t address =
        (1U << 31) |
        ((uint32_t)bus  << 16) |
        ((uint32_t)dev  << 11) |
        ((uint32_t)func << 8)  |
        (off & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t old = inl(PCI_CONFIG_DATA);
    if (off & 2) old = (old & 0x0000FFFFU) | ((uint32_t)val << 16);
    else         old = (old & 0xFFFF0000U) | val;
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, old);
}

/* =====================================================================
 * Constantes AHCI
 * ===================================================================== */
#define AHCI_GHC_AE         (1U << 31)
#define AHCI_GHC_HR         (1U << 0)
#define AHCI_PXCMD_ST       (1U << 0)
#define AHCI_PXCMD_SUD      (1U << 1)
#define AHCI_PXCMD_POD      (1U << 2)
#define AHCI_PXCMD_FRE      (1U << 4)
#define AHCI_PXCMD_FR       (1U << 14)
#define AHCI_PXCMD_CR       (1U << 15)
#define AHCI_PXIS_TFES      (1U << 30)
#define AHCI_TFD_BSY        (1U << 7)
#define AHCI_TFD_DRQ        (1U << 3)

#define SATA_DET_NONE       0x0
#define SATA_DET_PRESENT    0x3
#define SATA_IPM_ACTIVE     0x1
#define SATA_SIG_ATA        0x00000101
#define SATA_SIG_ATAPI      0xEB140101
#define SATA_SIG_SEMB       0xC33C0101
#define SATA_SIG_PM         0x96690101

#define FIS_TYPE_REG_H2D    0x27
#define ATA_CMD_IDENTIFY    0xEC

/* =====================================================================
 * Estructuras AHCI
 * ===================================================================== */
struct HBA_PORT {
    volatile uint32_t clb, clbu, fb, fbu;
    volatile uint32_t is, ie, cmd, reserved0;
    volatile uint32_t tfd, sig, ssts, sctl;
    volatile uint32_t serr, sact, ci, sntf, fbs;
    volatile uint32_t reserved1[11];
    volatile uint32_t vendor[4];
} __attribute__((packed));

struct HBA_MEM {
    volatile uint32_t cap, ghc, is, pi, vs;
    volatile uint32_t ccc_ctl, ccc_pts, em_loc, em_ctl, cap2, bohc;
    uint8_t reserved[0xA0 - 0x2C];
    uint8_t vendor[0x100 - 0xA0];
    HBA_PORT ports[32];
} __attribute__((packed));

struct HBA_CMD_HEADER {
    uint32_t dw0;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} __attribute__((packed));

struct HBA_PRDT {
    uint32_t dba, dbau, reserved, dbc;
} __attribute__((packed));

struct HBA_CMD_TABLE {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  reserved[48];
    HBA_PRDT prdt[1];
} __attribute__((packed));

/* =====================================================================
 * Buffers DMA alineados
 * ===================================================================== */
static HBA_CMD_HEADER ahci_cmd_list[32]      __attribute__((aligned(1024)));
static uint8_t        ahci_received_fis[256] __attribute__((aligned(256)));
static HBA_CMD_TABLE  ahci_cmd_table         __attribute__((aligned(128)));
static uint16_t       ahci_identify[256]     __attribute__((aligned(512)));

/* =====================================================================
 * Helpers
 * ===================================================================== */
static void memzero(void *p, uint32_t n) {
    uint8_t *b = (uint8_t *)p;
    for (uint32_t i = 0; i < n; i++) b[i] = 0;
}

static void wait_ms(uint32_t ms) {
    /* Aproximado; si tienes timer, reemplaza. */
    for (volatile uint32_t i = 0; i < ms * 10000; i++) ;
}

static const char *sig_name(uint32_t sig) {
    switch (sig) {
        case SATA_SIG_ATA:   return "ATA";
        case SATA_SIG_ATAPI: return "ATAPI";
        case SATA_SIG_SEMB:  return "SEMB";
        case SATA_SIG_PM:    return "PM";
        default:             return "?";
    }
}

static const char *det_name(uint8_t det) {
    switch (det) {
        case 0x0: return "none";
        case 0x1: return "present-no-comm";
        case 0x3: return "present";
        case 0x4: return "offline";
        default:  return "?";
    }
}

/* =====================================================================
 * Búsqueda PCI
 * ===================================================================== */
static int ahci_find_pci(uint8_t *out_bus, uint8_t *out_dev, uint8_t *out_func)
{
    for (uint32_t bus = 0; bus < 256; bus++)
    for (uint32_t dev = 0; dev < 32; dev++)
    for (uint32_t func = 0; func < 8; func++) {
        uint32_t id = pci_read32(bus, dev, func, 0x00);
        if ((id & 0xFFFF) == 0xFFFF || (id & 0xFFFF) == 0x0000) continue;

        uint32_t cls = pci_read32(bus, dev, func, 0x08);
        uint8_t progif = (cls >> 8)  & 0xFF;
        uint8_t subcls = (cls >> 16) & 0xFF;
        uint8_t classc = (cls >> 24) & 0xFF;

        if (classc == PCI_CLASS_STORAGE &&
            subcls == PCI_SUBCLASS_AHCI &&
            progif == PCI_PROGIF_AHCI)
        {
            *out_bus  = (uint8_t)bus;
            *out_dev  = (uint8_t)dev;
            *out_func = (uint8_t)func;
            kprintf("  PCI AHCI: %x:%x.%x  vendor=%x device=%x\n",
                    bus, dev, func, id & 0xFFFF, id >> 16);
            return 1;
        }
    }
    return 0;
}

static int ahci_enable_pci(uint8_t bus, uint8_t dev, uint8_t func)
{
    uint16_t cmd = pci_read16(bus, dev, func, 0x04);
    kprintf("  PCI CMD antes: %x\n", cmd);
    cmd |= (1 << 1) | (1 << 2);  /* MEM + BM */
    pci_write16(bus, dev, func, 0x04, cmd);
    cmd = pci_read16(bus, dev, func, 0x04);
    kprintf("  PCI CMD despues: %x\n", cmd);
    return (cmd & (1 << 1)) && (cmd & (1 << 2));
}

static HBA_MEM *ahci_map_abar(uint8_t bus, uint8_t dev, uint8_t func)
{
    uint32_t bar5 = pci_read32(bus, dev, func, 0x24);
    kprintf("  BAR5 = %x\n", bar5);

    if (bar5 & 1) {
        kprintf("  ERROR: BAR5 es I/O\n");
        return 0;
    }

    /* BAR5 puede ser 64-bit (type = 2). En ese caso BAR6 = high. */
    uint32_t type = (bar5 >> 1) & 0x3;
    uint32_t abar = bar5 & 0xFFFFFFF0U;
    if (type == 0x2) {
        uint32_t bar6 = pci_read32(bus, dev, func, 0x28);
        kprintf("  BAR5 es 64-bit, BAR6 = %x\n", bar6);
        /* En x86-32 asumimos que BAR6 = 0 */
    }

    if (!abar) {
        kprintf("  ERROR: ABAR = 0\n");
        return 0;
    }

    kprintf("  ABAR = %x  (mapeando 32 KiB uncached)\n", abar);
    paging_map_range(abar, abar, 32, PTE_PRESENT | PTE_RW | PTE_PCD | PTE_PWT);
    return (HBA_MEM *)abar;
}

/* =====================================================================
 * Puertos
 * ===================================================================== */
static int ahci_port_is_sata(HBA_PORT *p)
{
    uint32_t ssts = p->ssts;
    uint8_t det = ssts & 0x0F;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    if (det != SATA_DET_PRESENT) return 0;
    if (ipm != SATA_IPM_ACTIVE)  return 0;
    if (p->sig == SATA_SIG_ATAPI) return 0;
    if (p->sig == SATA_SIG_SEMB)  return 0;
    if (p->sig == SATA_SIG_PM)    return 0;
    if (p->sig != SATA_SIG_ATA)   return 0;
    return 1;
}

static int ahci_wait_clear(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t t = 10000000;
    while ((*reg & mask) != 0) {
        if (--t == 0) return 0;
    }
    return 1;
}

static int ahci_stop_port(HBA_PORT *port)
{
    port->cmd &= ~AHCI_PXCMD_ST;
    if (!ahci_wait_clear(&port->cmd, AHCI_PXCMD_CR)) return 0;
    port->cmd &= ~AHCI_PXCMD_FRE;
    uint32_t t = 10000000;
    while (port->cmd & (AHCI_PXCMD_FR | AHCI_PXCMD_CR)) {
        if (--t == 0) return 0;
    }
    return 1;
}

static int ahci_start_port(HBA_PORT *port)
{
    uint32_t t = 10000000;
    while (port->cmd & AHCI_PXCMD_CR) {
        if (--t == 0) return 0;
    }
    port->cmd |= AHCI_PXCMD_FRE;
    port->cmd |= AHCI_PXCMD_ST;
    return 1;
}

static int ahci_setup_port(HBA_PORT *port)
{
    if (!ahci_stop_port(port)) return 0;

    memzero(ahci_cmd_list, sizeof(ahci_cmd_list));
    memzero(ahci_received_fis, sizeof(ahci_received_fis));
    memzero(&ahci_cmd_table, sizeof(ahci_cmd_table));

    port->clb  = (uint32_t)&ahci_cmd_list[0];
    port->clbu = 0;
    port->fb   = (uint32_t)&ahci_received_fis[0];
    port->fbu  = 0;

    HBA_CMD_HEADER *h = &ahci_cmd_list[0];
    h->dw0   = 5 | (1U << 16);
    h->prdbc = 0;
    h->ctba  = (uint32_t)&ahci_cmd_table;
    h->ctbau = 0;

    port->is   = 0xFFFFFFFF;
    port->serr = 0xFFFFFFFF;
    port->ie   = 0;

    return ahci_start_port(port);
}

static int ahci_wait_not_busy(HBA_PORT *port)
{
    uint32_t t = 10000000;
    while (port->tfd & (AHCI_TFD_BSY | AHCI_TFD_DRQ)) {
        if (--t == 0) return 0;
    }
    return 1;
}

/* =====================================================================
 * IDENTIFY DEVICE (solo lectura)
 * ===================================================================== */
static int ahci_identify(HBA_PORT *port, uint16_t *out)
{
    memzero(out, 512);
    memzero(&ahci_cmd_table, sizeof(ahci_cmd_table));

    if (!ahci_wait_not_busy(port)) {
        kprintf("  IDENTIFY: TFD ocupado\n");
        return 0;
    }

    HBA_CMD_HEADER *h = &ahci_cmd_list[0];
    h->dw0   = 5 | (1U << 16);
    h->prdbc = 0;
    h->ctba  = (uint32_t)&ahci_cmd_table;
    h->ctbau = 0;

    HBA_PRDT *prdt = &ahci_cmd_table.prdt[0];
    prdt->dba  = (uint32_t)out;
    prdt->dbau = 0;
    prdt->dbc  = 511 & 0x003FFFFFU;

    uint8_t *fis = ahci_cmd_table.cfis;
    memzero(fis, 64);
    fis[0] = FIS_TYPE_REG_H2D;
    fis[1] = 0x80;                /* command */
    fis[2] = ATA_CMD_IDENTIFY;    /* 0xEC */
    fis[7] = 0;                   /* device: LBA */

    port->is   = 0xFFFFFFFF;
    port->serr = 0xFFFFFFFF;
    port->ci   = 1;

    uint32_t timeout = 50000000;
    while (port->ci & 1) {
        if (port->is & AHCI_PXIS_TFES) {
            kprintf("  IDENTIFY: TFES IS=%x TFD=%x SERR=%x\n",
                    port->is, port->tfd, port->serr);
            port->is = 0xFFFFFFFF; port->serr = 0xFFFFFFFF;
            return 0;
        }
        if (--timeout == 0) {
            kprintf("  IDENTIFY: timeout CI=%x IS=%x TFD=%x\n",
                    port->ci, port->is, port->tfd);
            return 0;
        }
    }

    if (!ahci_wait_not_busy(port)) return 0;

    if (port->is & AHCI_PXIS_TFES) {
        kprintf("  IDENTIFY: TFES post IS=%x TFD=%x\n", port->is, port->tfd);
        port->is = 0xFFFFFFFF; port->serr = 0xFFFFFFFF;
        return 0;
    }

    port->is = 0xFFFFFFFF;
    port->serr = 0xFFFFFFFF;
    return 1;
}

/* =====================================================================
 * Impresión de strings ATA (word-swapped)
 * ===================================================================== */
static void ata_str(uint16_t *id, int word_start, int word_count, char *out)
{
    int k = 0;
    for (int i = 0; i < word_count; i++) {
        uint16_t w = id[word_start + i];
        out[k++] = (char)(w >> 8);
        out[k++] = (char)(w & 0xFF);
    }
    out[k] = 0;
    /* trim trailing spaces */
    for (int i = k - 1; i >= 0; i--) {
        if (out[i] == ' ' || out[i] == '\0') out[i] = 0;
        else break;
    }
}

static void print_hex16(uint16_t v) {
    const char *h = "0123456789ABCDEF";
    char b[5];
    b[0] = h[(v >> 12) & 0xF];
    b[1] = h[(v >> 8)  & 0xF];
    b[2] = h[(v >> 4)  & 0xF];
    b[3] = h[v & 0xF];
    b[4] = 0;
    kprintf("%s", b);
}

/* =====================================================================
 * Test principal
 * ===================================================================== */
extern "C" void test_ahci_run(void)
{
    kprintf("\n");
    kprintf("========================================\n");
    kprintf("  TEST AHCI - SOLO LECTURA\n");
    kprintf("  (no se escribe nada al disco)\n");
    kprintf("========================================\n");

    /* ---------- 1. Buscar en PCI ---------- */
    kprintf("\n[1] Buscando controlador AHCI en PCI...\n");
    uint8_t bus, dev, func;
    if (!ahci_find_pci(&bus, &dev, &func)) {
        kprintf("  NO se encontro controlador AHCI\n");
        kprintf("  (revisa BIOS: modo SATA debe ser AHCI, no IDE/RAID)\n");
        return;
    }

    /* ---------- 2. Habilitar PCI ---------- */
    kprintf("\n[2] Habilitando MEM + Bus Master...\n");
    if (!ahci_enable_pci(bus, dev, func)) {
        kprintf("  ERROR habilitando PCI\n");
        return;
    }

    /* ---------- 3. Mapear ABAR ---------- */
    kprintf("\n[3] Mapeando ABAR...\n");
    HBA_MEM *hba = ahci_map_abar(bus, dev, func);
    if (!hba) { kprintf("  ERROR mapeando ABAR\n"); return; }

    /* ---------- 4. Activar HBA ---------- */
    kprintf("\n[4] Activando HBA (AE)...\n");
    hba->ghc |= AHCI_GHC_AE;
    if (!(hba->ghc & AHCI_GHC_AE)) {
        kprintf("  ERROR: AE no se activo (GHC=%x)\n", hba->ghc);
        return;
    }

    kprintf("  CAP  = %x\n", hba->cap);
    kprintf("  GHC  = %x\n", hba->ghc);
    kprintf("  IS   = %x\n", hba->is);
    kprintf("  PI   = %x  (puertos implementados)\n", hba->pi);
    kprintf("  VS   = %x  (version)\n", hba->vs);

    uint8_t n_ports = (hba->cap & 0x1F) + 1;
    kprintf("  Num puertos: %u\n", n_ports);

    /* ---------- 5. Enumerar TODOS los puertos ---------- */
    kprintf("\n[5] Enumerando puertos:\n");
    kprintf("  puerto  DET    IPM    SIG          estado\n");
    for (int i = 0; i < 32; i++) {
        if (!(hba->pi & (1U << i))) continue;
        HBA_PORT *p = &hba->ports[i];
        uint32_t ssts = p->ssts;
        uint8_t det = ssts & 0x0F;
        uint8_t ipm = (ssts >> 8) & 0x0F;
        kprintf("  %u       %s   %x      %x       %s\n",
                i, det_name(det), ipm, p->sig,
                ahci_port_is_sata(p) ? "SATA OK" : "no usable");
    }

    /* ---------- 6. Elegir primer SATA válido ---------- */
    HBA_PORT *port = 0;
    int port_idx = -1;
    for (int i = 0; i < 32; i++) {
        if (!(hba->pi & (1U << i))) continue;
        if (ahci_port_is_sata(&hba->ports[i])) {
            port = &hba->ports[i];
            port_idx = i;
            break;
        }
    }
    if (!port) {
        kprintf("\n  NO hay disco SATA conectado\n");
        return;
    }
    kprintf("\n[6] Disco SATA encontrado en puerto %d\n", port_idx);
    kprintf("  SIG  = %x (%s)\n", port->sig, sig_name(port->sig));
    kprintf("  SSTS = %x\n", port->ssts);

    /* ---------- 7. Setup puerto ---------- */
    kprintf("\n[7] Inicializando puerto...\n");
    if (!ahci_setup_port(port)) {
        kprintf("  ERROR inicializando puerto\n");
        return;
    }
    kprintf("  CLB=%x  FB=%x  CTBA=%x\n",
            port->clb, port->fb, (uint32_t)&ahci_cmd_table);
    kprintf("  PXCMD=%x\n", port->cmd);

    /* ---------- 8. IDENTIFY DEVICE ---------- */
    kprintf("\n[8] IDENTIFY DEVICE...\n");
    if (!ahci_identify(port, ahci_identify)) {
        kprintf("  ERROR en IDENTIFY\n");
        return;
    }

    char model[41], serial[21], firmware[9];

    ata_str(ahci_identify, 27, 20, model);     /* modelo  40 bytes */
    ata_str(ahci_identify, 10, 10, serial);    /* serie   20 bytes */
    ata_str(ahci_identify, 23,  4, firmware);  /* firmware 8 bytes */

    kprintf("\n  ===== DATOS DEL DISCO =====\n");
    kprintf("  Modelo   : %s\n", model);
    kprintf("  Serie    : %s\n", serial);
    kprintf("  Firmware : %s\n", firmware);

    uint16_t w83 = ahci_identify[83];
    kprintf("  Word 83  : %x\n", w83);
    kprintf("    LBA48  : %s\n", (w83 & (1 << 10)) ? "SI" : "NO");
    kprintf("    DMA    : %s\n", (w83 & (1 << 15)) ? "SI" : "NO");
    kprintf("    NCQ    : %s\n", (ahci_identify[76] & (1 << 8)) ? "SI" : "NO");

    int lba48 = (w83 & (1 << 10)) != 0;

    uint64_t sectors = 0;
    if (lba48) {
        sectors =
            ((uint64_t)ahci_identify[103] << 48) |
            ((uint64_t)ahci_identify[102] << 32) |
            ((uint64_t)ahci_identify[101] << 16) |
            ((uint64_t)ahci_identify[100]);
        kprintf("  LBA48    : %u sectores\n", (uint32_t)sectors);
        kprintf("  LBA48 hi : %u\n", (uint32_t)(sectors >> 32));
    } else {
        sectors =
            ((uint32_t)ahci_identify[61] << 16) |
            ((uint32_t)ahci_identify[60]);
        kprintf("  LBA28    : %u sectores\n", (uint32_t)sectors);
    }

    uint64_t bytes = sectors * 512ULL;
    uint32_t mib   = (uint32_t)(bytes / (1024ULL * 1024ULL));
    uint32_t gib   = (uint32_t)(bytes / (1024ULL * 1024ULL * 1024ULL));
    kprintf("  Bytes    : %u\n", (uint32_t)bytes);
    kprintf("  Tamano   : %u MiB  (~%u GiB)\n", mib, gib);

    uint16_t w106 = ahci_identify[106];
    if (w106 & (1 << 12)) {
        kprintf("  Sector   : 4096 bytes (logico)\n");
    } else {
        kprintf("  Sector   : 512 bytes (logico)\n");
    }

    uint16_t w0 = ahci_identify[0];
    kprintf("  Word 0   : %x  %s\n", w0,
            (w0 & 0x8000) ? "(ATAPI)" : "(ATA)");

    kprintf("\n  ===== FIN TEST AHCI =====\n");
    kprintf("  Disco detectado correctamente en modo lectura.\n");
    kprintf("========================================\n\n");
}
