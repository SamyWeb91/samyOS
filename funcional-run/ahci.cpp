/*
 * ============================================================
 *  ahci.cpp  –  Mini driver AHCI 32-bit (lectura + escritura)
 *  Mejorado: espera BSY/DRQ tras cada comando
 * ============================================================
 */
#include <stdint.h>
#include "screen.h"
#include "io.h"
#include "paging.h"
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

#define AHCI_CLASS          0x01
#define AHCI_SUBCLASS       0x06
#define AHCI_PROGIF         0x01

#define AHCI_GHC_AE         (1U << 31)
#define AHCI_GHC_HR         (1U << 0)

#define AHCI_PXCMD_ST       (1U << 0)
#define AHCI_PXCMD_FRE      (1U << 4)
#define AHCI_PXCMD_FR       (1U << 14)
#define AHCI_PXCMD_CR       (1U << 15)
#define AHCI_PXIS_TFES      (1U << 30)
#define AHCI_TFD_BSY        (1U << 7)
#define AHCI_TFD_DRQ        (1U << 3)

#define SATA_DET_PRESENT    0x3
#define SATA_IPM_ACTIVE     0x1
#define SATA_SIG_ATAPI      0xEB140101

#define FIS_TYPE_REG_H2D    0x27
#define ATA_CMD_IDENTIFY       0xEC
#define ATA_CMD_READ_DMA_EXT   0x25
#define ATA_CMD_WRITE_DMA_EXT  0x35

/* ---------------- PCI ---------------- */
static uint32_t pci_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t address =
        (1U << 31) |
        ((uint32_t)bus      << 16) |
        ((uint32_t)device   << 11) |
        ((uint32_t)function << 8) |
        (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static uint16_t pci_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t value = pci_read32(bus, device, function, offset);
    if (offset & 2) return (uint16_t)(value >> 16);
    return (uint16_t)(value & 0xFFFF);
}

static void pci_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value)
{
    uint32_t address =
        (1U << 31) |
        ((uint32_t)bus      << 16) |
        ((uint32_t)device   << 11) |
        ((uint32_t)function << 8) |
        (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t old = inl(PCI_CONFIG_DATA);
    if (offset & 2) old = (old & 0x0000FFFFU) | ((uint32_t)value << 16);
    else            old = (old & 0xFFFF0000U) | value;
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, old);
}

/* ---------------- estructuras AHCI ---------------- */
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

/* ---------------- DMA ---------------- */
static HBA_CMD_HEADER ahci_cmd_list[32]      __attribute__((aligned(1024)));
static uint8_t        ahci_received_fis[256] __attribute__((aligned(256)));
static HBA_CMD_TABLE  ahci_cmd_table         __attribute__((aligned(128)));
static uint8_t        ahci_dma_buffer[512]   __attribute__((aligned(512)));
static uint16_t       ahci_identify_data[256] __attribute__((aligned(512)));

/* ---------------- estado ---------------- */
static HBA_MEM  *ahci_hba  = 0;
static HBA_PORT *ahci_port = 0;
static uint8_t   ahci_pci_bus = 0, ahci_pci_dev = 0, ahci_pci_func = 0;
static uint64_t  ahci_sector_count = 0;
static int       ahci_ready = 0;

/* ---------------- helpers ---------------- */
static void memzero(void *ptr, uint32_t size)
{
    uint8_t *p = (uint8_t *)ptr;
    for (uint32_t i = 0; i < size; i++) p[i] = 0;
}

static void memcpy8(void *dst, const void *src, uint32_t size)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < size; i++) d[i] = s[i];
}

static int ahci_wait_clear(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t t = 10000000;
    while ((*reg & mask) != 0) {
        if (--t == 0) return 0;
    }
    return 1;
}

/* ---------------- PCI ---------------- */
static int ahci_find_pci()
{
    for (uint32_t bus = 0; bus < 256; bus++)
    for (uint32_t dev = 0; dev < 32; dev++)
    for (uint32_t func = 0; func < 8; func++) {
        uint32_t id = pci_read32((uint8_t)bus, (uint8_t)dev, (uint8_t)func, 0x00);
        if ((id & 0xFFFF) == 0xFFFF) continue;

        uint32_t class_reg = pci_read32((uint8_t)bus, (uint8_t)dev, (uint8_t)func, 0x08);
        uint8_t prog_if    = (class_reg >> 8)  & 0xFF;
        uint8_t subclass   = (class_reg >> 16) & 0xFF;
        uint8_t class_code = (class_reg >> 24) & 0xFF;

        if (class_code == AHCI_CLASS && subclass == AHCI_SUBCLASS && prog_if == AHCI_PROGIF) {
            ahci_pci_bus  = (uint8_t)bus;
            ahci_pci_dev  = (uint8_t)dev;
            ahci_pci_func = (uint8_t)func;
            kprintf("AHCI: PCI %x:%x.%x vendor=%x device=%x\n",
                    bus, dev, func, id & 0xFFFF, id >> 16);
            return 1;
        }
    }
    return 0;
}

static int ahci_enable_pci()
{
    uint16_t command = pci_read16(ahci_pci_bus, ahci_pci_dev, ahci_pci_func, 0x04);
    command |= (1 << 1) | (1 << 2);
    pci_write16(ahci_pci_bus, ahci_pci_dev, ahci_pci_func, 0x04, command);
    command = pci_read16(ahci_pci_bus, ahci_pci_dev, ahci_pci_func, 0x04);
    if ((command & (1 << 1)) == 0 || (command & (1 << 2)) == 0) {
        kprintf("AHCI: fallo habilitando PCI\n");
        return 0;
    }
    return 1;
}
//he modificado esta parte

static HBA_MEM *ahci_get_abar()
{
    uint32_t bar5 = pci_read32(ahci_pci_bus, ahci_pci_dev, ahci_pci_func, 0x24);

    // bit 0 = 1 → BAR de I/O, no MMIO
    if (bar5 & 1) {
        kprintf("AHCI: BAR5 es I/O, no MMIO\n");
        return 0;
    }

    uint32_t abar = bar5 & 0xFFFFFFF0U;

    if (!abar) {
        kprintf("AHCI: ABAR = 0\n");
        return 0;
    }

    kprintf("AHCI: ABAR = %x\n", abar);

    // >>> FIX: mapear dinámicamente el ABAR en el espacio virtual.
    //
    // En QEMU, el ABAR suele estar en 0xFEBD5000 (dentro del rango
    // MMIO clásico 0xFE000000-0xFFFFFFFF que el kernel mapea al
    // arranque). Pero en hardware real (ej. Intel Sunrise Point-LP,
    // device 0x9D03), el ABAR puede estar en 0xF124C000, FUERA de
    // ese rango. Sin mapearlo, el primer acceso provoca Page Fault.
    //
    // Mapeamos 32 KB desde el ABAR (más que suficiente para los
    // registros AHCI: HBA_MEM ocupa ~0x1100 bytes + ports).
    //
    // paging_map_range(virt, phys, size_kb, flags):
    //   - Como estamos en identity mapping, virt == phys.
    //   - size_kb = 32 → mapea 32 páginas de 4 KB = 128 KB.
    //
    // Nota: si el ABAR cruza un límite de 4 MB, paging_map() crea
    // automáticamente una nueva page table en pt_pool.
    paging_map_range(abar, abar, 32, PTE_PRESENT | PTE_RW);

    kprintf("AHCI: ABAR mapeado (%u KB)\n", 32);

    return (HBA_MEM *)abar;
}

/* ---------------- puerto SATA ---------------- */
static int ahci_port_is_sata(HBA_PORT *port)
{
    uint32_t ssts = port->ssts;
    uint8_t det = ssts & 0x0F;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    if (det != SATA_DET_PRESENT || ipm != SATA_IPM_ACTIVE) return 0;
    if (port->sig == SATA_SIG_ATAPI) return 0;
    return 1;
}

static int ahci_find_port()
{
    uint32_t pi = ahci_hba->pi;
    for (int i = 0; i < 32; i++) {
        if ((pi & (1U << i)) == 0) continue;
        HBA_PORT *port = &ahci_hba->ports[i];
        kprintf("AHCI: puerto %d DET=%x IPM=%x SIG=%x\n",
                i, port->ssts & 0xF, (port->ssts >> 8) & 0xF, port->sig);
        if (ahci_port_is_sata(port)) {
            ahci_port = port;
            kprintf("AHCI: disco SATA en puerto %d\n", i);
            return 1;
        }
    }
    return 0;
}

static int ahci_stop_port(HBA_PORT *port)
{
    port->cmd &= ~AHCI_PXCMD_ST;
    if (!ahci_wait_clear(&port->cmd, AHCI_PXCMD_CR)) {
        kprintf("AHCI: timeout CR=0\n");
        return 0;
    }
    port->cmd &= ~AHCI_PXCMD_FRE;
    uint32_t t = 10000000;
    while (port->cmd & (AHCI_PXCMD_FR | AHCI_PXCMD_CR)) {
        if (--t == 0) { kprintf("AHCI: timeout stop puerto\n"); return 0; }
    }
    return 1;
}

static int ahci_start_port(HBA_PORT *port)
{
    uint32_t t = 10000000;
    while (port->cmd & AHCI_PXCMD_CR) {
        if (--t == 0) { kprintf("AHCI: timeout command engine\n"); return 0; }
    }
    port->cmd |= AHCI_PXCMD_FRE;
    port->cmd |= AHCI_PXCMD_ST;
    return 1;
}

static int ahci_setup_port()
{
    if (!ahci_stop_port(ahci_port)) return 0;

    memzero(ahci_cmd_list, sizeof(ahci_cmd_list));
    memzero(ahci_received_fis, sizeof(ahci_received_fis));
    memzero(&ahci_cmd_table, sizeof(ahci_cmd_table));

    uint32_t clb  = (uint32_t)&ahci_cmd_list[0];
    uint32_t fb   = (uint32_t)&ahci_received_fis[0];
    uint32_t ctba = (uint32_t)&ahci_cmd_table;

    ahci_port->clb  = clb;
    ahci_port->clbu = 0;
    ahci_port->fb   = fb;
    ahci_port->fbu  = 0;

    HBA_CMD_HEADER *header = &ahci_cmd_list[0];
    header->dw0   = 5 | (1U << 16);
    header->prdbc = 0;
    header->ctba  = ctba;
    header->ctbau = 0;

    ahci_port->is   = 0xFFFFFFFF;
    ahci_port->serr = 0xFFFFFFFF;
    ahci_port->ie   = 0;

    if (!ahci_start_port(ahci_port)) return 0;

    kprintf("AHCI: puerto listo CLB=%x FB=%x CTBA=%x\n", clb, fb, ctba);
    return 1;
}

static int ahci_wait_not_busy()
{
    uint32_t t = 10000000;
    while (ahci_port->tfd & (AHCI_TFD_BSY | AHCI_TFD_DRQ)) {
        if (--t == 0) { kprintf("AHCI: timeout TFD BSY/DRQ\n"); return 0; }
    }
    return 1;
}

/* ---------------- comando ATA (slot 0) ---------------- */
static int ahci_issue_command(uint8_t command, uint64_t lba, uint16_t sector_count)
{
    if (!ahci_port || sector_count == 0) {
        kprintf("AHCI: issue: port=%x count=%u\n",
                (uint32_t)ahci_port, sector_count);
        return 0;
    }

    /* Esperar a que el disco esté listo ANTES */
    if (!ahci_wait_not_busy()) return 0;

    /* Esperar a que CI y SACT estén libres (con timeout) */
    {
        uint32_t t = 10000000;
        while ((ahci_port->ci & 1) || (ahci_port->sact & 1)) {
            if (--t == 0) {
                kprintf("AHCI: slot ocupado CI=%x SACT=%x\n",
                        ahci_port->ci, ahci_port->sact);
                return 0;
            }
        }
    }

    /* Limpiar estado */
    ahci_port->is   = 0xFFFFFFFF;
    ahci_port->serr = 0xFFFFFFFF;

    memzero(&ahci_cmd_table, sizeof(ahci_cmd_table));

    HBA_CMD_HEADER *header = &ahci_cmd_list[0];
    header->dw0 = 5 | (1U << 16);
    if (command == ATA_CMD_WRITE_DMA_EXT)
        header->dw0 |= (1U << 6);
    header->prdbc = 0;
    header->ctba  = (uint32_t)&ahci_cmd_table;
    header->ctbau = 0;

    HBA_PRDT *prdt = &ahci_cmd_table.prdt[0];
    prdt->dba      = (uint32_t)&ahci_dma_buffer[0];
    prdt->dbau     = 0;
    prdt->reserved = 0;
    /* Sin IOC */
    prdt->dbc      = 511U & 0x003FFFFFU;

    uint8_t *fis = ahci_cmd_table.cfis;
    memzero(fis, 64);
    fis[0]  = FIS_TYPE_REG_H2D;
    fis[1]  = 0x80;
    fis[2]  = command;
    fis[3]  = 0;
    fis[4]  = (uint8_t)(lba);
    fis[5]  = (uint8_t)(lba >> 8);
    fis[6]  = (uint8_t)(lba >> 16);
    fis[7]  = 0x40;
    fis[8]  = (uint8_t)(lba >> 24);
    fis[9]  = (uint8_t)(lba >> 32);
    fis[10] = (uint8_t)(lba >> 40);
    fis[11] = 0;
    fis[12] = (uint8_t)(sector_count);
    fis[13] = (uint8_t)(sector_count >> 8);

    ahci_port->ci = 1;

    uint32_t timeout = 50000000;
    while (ahci_port->ci & 1) {
        if (ahci_port->is & AHCI_PXIS_TFES) {
            kprintf("AHCI: TASK FILE ERROR IS=%x TFD=%x SERR=%x cmd=%x\n",
                    ahci_port->is, ahci_port->tfd, ahci_port->serr, command);
            ahci_port->is   = 0xFFFFFFFF;
            ahci_port->serr = 0xFFFFFFFF;
            return 0;
        }
        if (--timeout == 0) {
            kprintf("AHCI: timeout cmd=%x CI=%x IS=%x TFD=%x\n",
                    command, ahci_port->ci, ahci_port->is, ahci_port->tfd);
            return 0;
        }
    }

    /* >>> FIX CLAVE: esperar a que el disco termine (BSY=0, DRQ=0) <<< */
    if (!ahci_wait_not_busy()) return 0;

    if (ahci_port->is & AHCI_PXIS_TFES) {
        kprintf("AHCI: TFES post-cmd IS=%x TFD=%x SERR=%x\n",
                ahci_port->is, ahci_port->tfd, ahci_port->serr);
        ahci_port->is   = 0xFFFFFFFF;
        ahci_port->serr = 0xFFFFFFFF;
        return 0;
    }

    ahci_port->is   = 0xFFFFFFFF;
    ahci_port->serr = 0xFFFFFFFF;
    return 1;
}

/* ---------------- IDENTIFY ---------------- */
static int ahci_identify()
{
    memzero(ahci_identify_data, sizeof(ahci_identify_data));
    memzero(ahci_dma_buffer, sizeof(ahci_dma_buffer));

    HBA_CMD_HEADER *header = &ahci_cmd_list[0];
    header->dw0   = 5 | (1U << 16);
    header->prdbc = 0;
    header->ctba  = (uint32_t)&ahci_cmd_table;
    header->ctbau = 0;

    memzero(&ahci_cmd_table, sizeof(ahci_cmd_table));
    HBA_PRDT *prdt = &ahci_cmd_table.prdt[0];
    prdt->dba  = (uint32_t)&ahci_identify_data[0];
    prdt->dbau = 0;
    prdt->dbc  = 511U & 0x003FFFFFU;

    uint8_t *fis = ahci_cmd_table.cfis;
    memzero(fis, 64);
    fis[0] = FIS_TYPE_REG_H2D;
    fis[1] = 0x80;
    fis[2] = ATA_CMD_IDENTIFY;
    fis[7] = 0;

    if (!ahci_wait_not_busy()) return 0;

    ahci_port->is   = 0xFFFFFFFF;
    ahci_port->serr = 0xFFFFFFFF;
    ahci_port->ci   = 1;

    uint32_t timeout = 50000000;
    while (ahci_port->ci & 1) {
        if (ahci_port->is & AHCI_PXIS_TFES) {
            kprintf("AHCI: IDENTIFY TFES\n");
            return 0;
        }
        if (--timeout == 0) {
            kprintf("AHCI: timeout IDENTIFY\n");
            return 0;
        }
    }

    if (!ahci_wait_not_busy()) return 0;

    uint16_t cmd_sets = ahci_identify_data[83];
    if (cmd_sets & (1 << 10)) {
        ahci_sector_count =
            ((uint64_t)ahci_identify_data[103] << 48) |
            ((uint64_t)ahci_identify_data[102] << 32) |
            ((uint64_t)ahci_identify_data[101] << 16) |
            ((uint64_t)ahci_identify_data[100]);
    } else {
        ahci_sector_count =
            ((uint32_t)ahci_identify_data[61] << 16) |
            ((uint32_t)ahci_identify_data[60]);
    }

    char model[41];
    for (int i = 0; i < 40; i += 2) {
        uint16_t w = ahci_identify_data[27 + i / 2];
        model[i]     = (char)(w >> 8);
        model[i + 1] = (char)(w & 0xFF);
    }
    model[40] = 0;
    for (int i = 39; i >= 0; i--) {
        if (model[i] == ' ' || model[i] == '\0') model[i] = '\0';
        else break;
    }

    kprintf("AHCI: IDENTIFY OK modelo='%s' sectores=%u\n",
            model, (uint32_t)ahci_sector_count);
    kprintf("AHCI: capacidad ~%u MiB\n",
            (uint32_t)((ahci_sector_count * 512ULL) / (1024ULL * 1024ULL)));

    if (ahci_sector_count == 0) {
        kprintf("AHCI: sector_count=0\n");
        return 0;
    }

    ahci_port->is   = 0xFFFFFFFF;
    ahci_port->serr = 0xFFFFFFFF;
    return 1;
}

/* ---------------- API pública ---------------- */
extern "C" int ahci_init()
{
    kprintf("\n============================\n AHCI DRIVER\n============================\n");
    ahci_ready = 0;
    ahci_sector_count = 0;

    if (!ahci_find_pci())      { kprintf("AHCI: no PCI\n"); return 0; }
    if (!ahci_enable_pci())    { kprintf("AHCI: PCI enable fail\n"); return 0; }
    ahci_hba = ahci_get_abar();
    if (!ahci_hba)             { kprintf("AHCI: ABAR fail\n"); return 0; }

    kprintf("AHCI: CAP=%x PI=%x VS=%x\n", ahci_hba->cap, ahci_hba->pi, ahci_hba->vs);

    ahci_hba->ghc |= AHCI_GHC_AE;
    if ((ahci_hba->ghc & AHCI_GHC_AE) == 0) {
        kprintf("AHCI: no AE\n");
        return 0;
    }

    if (!ahci_find_port())     { kprintf("AHCI: no SATA\n"); return 0; }
    if (!ahci_setup_port())    { kprintf("AHCI: setup port fail\n"); return 0; }
    if (!ahci_identify())      { kprintf("AHCI: IDENTIFY fail\n"); return 0; }

    ahci_ready = 1;
    kprintf("AHCI: DRIVER LISTO\n");
    return 1;
}

extern "C" int ahci_read_sector(uint64_t lba, void *buffer)
{
    if (!ahci_ready) { kprintf("AHCI: no ready (read)\n"); return 0; }
    if (!buffer)     { kprintf("AHCI: buffer NULL (read)\n"); return 0; }
    if (lba >= ahci_sector_count) {
        kprintf("AHCI: LBA fuera %u (max %u)\n",
                (uint32_t)lba, (uint32_t)ahci_sector_count);
        return 0;
    }

    memzero(ahci_dma_buffer, 512);
    if (!ahci_issue_command(ATA_CMD_READ_DMA_EXT, lba, 1)) {
        kprintf("AHCI: lectura fallida LBA=%u\n", (uint32_t)lba);
        return 0;
    }
    memcpy8(buffer, ahci_dma_buffer, 512);
    return 1;
}

extern "C" int ahci_write_sector(uint64_t lba, const void *buffer)
{
    if (!ahci_ready) { kprintf("AHCI: no ready (write)\n"); return 0; }
    if (!buffer)     { kprintf("AHCI: buffer NULL (write)\n"); return 0; }
    if (lba >= ahci_sector_count) {
        kprintf("AHCI: LBA fuera %u (max %u)\n",
                (uint32_t)lba, (uint32_t)ahci_sector_count);
        return 0;
    }

    memcpy8(ahci_dma_buffer, buffer, 512);

    if (!ahci_issue_command(ATA_CMD_WRITE_DMA_EXT, lba, 1)) {
        kprintf("AHCI: escritura fallida LBA=%u\n", (uint32_t)lba);
        return 0;
    }
    return 1;
}

extern "C" uint64_t ahci_get_sector_count() { return ahci_sector_count; }
extern "C" int      ahci_is_ready()        { return ahci_ready; }
