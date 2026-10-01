#include <stdint.h>
#include "screen.h"
#include "io.h"

#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

static uint32_t pci_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address =
        (1U << 31) |
        ((uint32_t)bus      << 16) |
        ((uint32_t)device   << 11) |
        ((uint32_t)function << 8)  |
        (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static uint16_t pci_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t value = pci_read32(bus, device, function, offset);
    if (offset & 2) return (uint16_t)(value >> 16);
    return (uint16_t)(value & 0xFFFF);
}

static void pci_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value) {
    uint32_t address =
        (1U << 31) |
        ((uint32_t)bus      << 16) |
        ((uint32_t)device   << 11) |
        ((uint32_t)function << 8)  |
        (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t old = inl(PCI_CONFIG_DATA);
    if (offset & 2) old = (old & 0x0000FFFFU) | ((uint32_t)value << 16);
    else            old = (old & 0xFFFF0000U) | value;
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, old);
}

#define AHCI_CLASS          0x01
#define AHCI_SUBCLASS       0x06
#define AHCI_PROGIF         0x01

#define AHCI_GHC_AE         (1U << 31)

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

#define ATA_CMD_IDENTIFY    0xEC
#define ATA_CMD_READ_DMA_EXT 0x25

struct HBA_PORT {
    volatile uint32_t clb;
    volatile uint32_t clbu;
    volatile uint32_t fb;
    volatile uint32_t fbu;
    volatile uint32_t is;
    volatile uint32_t ie;
    volatile uint32_t cmd;
    volatile uint32_t reserved0;
    volatile uint32_t tfd;
    volatile uint32_t sig;
    volatile uint32_t ssts;
    volatile uint32_t sctl;
    volatile uint32_t serr;
    volatile uint32_t sact;
    volatile uint32_t ci;
    volatile uint32_t sntf;
    volatile uint32_t fbs;
    volatile uint32_t reserved1[11];
    volatile uint32_t vendor[4];
} __attribute__((packed));

struct HBA_MEM {
    volatile uint32_t cap;
    volatile uint32_t ghc;
    volatile uint32_t is;
    volatile uint32_t pi;
    volatile uint32_t vs;
    volatile uint32_t ccc_ctl;
    volatile uint32_t ccc_pts;
    volatile uint32_t em_loc;
    volatile uint32_t em_ctl;
    volatile uint32_t cap2;
    volatile uint32_t bohc;
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
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved;
    uint32_t dbc;
} __attribute__((packed));

struct HBA_CMD_TABLE {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    HBA_PRDT prdt[1];
} __attribute__((packed));

static HBA_CMD_HEADER ahci_cmd_list[32]    __attribute__((aligned(1024)));
static uint8_t        ahci_received_fis[256] __attribute__((aligned(256)));
static HBA_CMD_TABLE  ahci_cmd_table         __attribute__((aligned(128)));
static uint8_t        ahci_dma_buffer[512]   __attribute__((aligned(512)));
static uint16_t       ahci_identify_data[256]__attribute__((aligned(512)));

static HBA_MEM  *ahci_hba  = 0;
static HBA_PORT *ahci_port = 0;

static uint8_t ahci_pci_bus  = 0;
static uint8_t ahci_pci_dev  = 0;
static uint8_t ahci_pci_func = 0;

static uint64_t ahci_sector_count = 0;
static int ahci_ready = 0;

static void memzero(void *ptr, uint32_t size) {
    uint8_t *p = (uint8_t *)ptr;
    for (uint32_t i = 0; i < size; i++) p[i] = 0;
}

static void memcpy8(void *dst, const void *src, uint32_t size) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < size; i++) d[i] = s[i];
}

static int ahci_wait_clear(volatile uint32_t *reg, uint32_t mask) {
    uint32_t timeout = 10000000;
    while ((*reg & mask) != 0) {
        if (--timeout == 0) return 0;
    }
    return 1;
}

static int ahci_find_pci() {
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t dev = 0; dev < 32; dev++) {
            for (uint32_t func = 0; func < 8; func++) {
                uint32_t id = pci_read32((uint8_t)bus, (uint8_t)dev, (uint8_t)func, 0x00);
                uint16_t vendor = id & 0xFFFF;
                uint16_t device = id >> 16;
                if (vendor == 0xFFFF) continue;

                uint32_t class_reg = pci_read32((uint8_t)bus, (uint8_t)dev, (uint8_t)func, 0x08);
                uint8_t prog_if    = (class_reg >> 8)  & 0xFF;
                uint8_t subclass   = (class_reg >> 16) & 0xFF;
                uint8_t class_code = (class_reg >> 24) & 0xFF;

                if (class_code == AHCI_CLASS && subclass == AHCI_SUBCLASS && prog_if == AHCI_PROGIF) {
                    ahci_pci_bus  = (uint8_t)bus;
                    ahci_pci_dev  = (uint8_t)dev;
                    ahci_pci_func = (uint8_t)func;
                    kprintf("AHCI: PCI %x:%x.%x vendor=%x device=%x\n", bus, dev, func, vendor, device);
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int ahci_enable_pci() {
    uint16_t command = pci_read16(ahci_pci_bus, ahci_pci_dev, ahci_pci_func, 0x04);
    command |= (1 << 1);
    command |= (1 << 2);
    pci_write16(ahci_pci_bus, ahci_pci_dev, ahci_pci_func, 0x04, command);
    command = pci_read16(ahci_pci_bus, ahci_pci_dev, ahci_pci_func, 0x04);

    if ((command & (1 << 1)) == 0) { kprintf("AHCI: fallo Memory Space\n"); return 0; }
    if ((command & (1 << 2)) == 0) { kprintf("AHCI: fallo Bus Master\n"); return 0; }
    return 1;
}

static HBA_MEM *ahci_get_abar() {
    uint32_t bar5 = pci_read32(ahci_pci_bus, ahci_pci_dev, ahci_pci_func, 0x24);
    if (bar5 & 1) { kprintf("AHCI: BAR5 es I/O\n"); return 0; }
    uint32_t abar = bar5 & 0xFFFFFFF0U;
    if (abar == 0) { kprintf("AHCI: ABAR=0\n"); return 0; }
    kprintf("AHCI: ABAR = %x\n", abar);
    return (HBA_MEM *)abar;
}

static int ahci_port_is_sata(HBA_PORT *port) {
    uint32_t ssts = port->ssts;
    uint8_t det = ssts & 0x0F;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    if (det != SATA_DET_PRESENT) return 0;
    if (ipm != SATA_IPM_ACTIVE) return 0;
    if (port->sig == SATA_SIG_ATAPI) return 0;
    return 1;
}

static int ahci_find_port() {
    if (!ahci_hba) return 0;
    uint32_t pi = ahci_hba->pi;
    for (int i = 0; i < 32; i++) {
        if ((pi & (1U << i)) == 0) continue;
        HBA_PORT *port = &ahci_hba->ports[i];
        uint32_t ssts = port->ssts;
        uint8_t det = ssts & 0x0F;
        uint8_t ipm = (ssts >> 8) & 0x0F;
        kprintf("AHCI: puerto %d DET=%x IPM=%x SIG=%x\n", i, det, ipm, port->sig);
        if (ahci_port_is_sata(port)) {
            ahci_port = port;
            kprintf("AHCI: disco SATA encontrado en puerto %d\n", i);
            return 1;
        }
    }
    return 0;
}

static int ahci_stop_port(HBA_PORT *port) {
    port->cmd &= ~AHCI_PXCMD_ST;
    if (!ahci_wait_clear(&port->cmd, AHCI_PXCMD_CR)) {
        kprintf("AHCI: timeout CR=0\n");
        return 0;
    }
    port->cmd &= ~AHCI_PXCMD_FRE;
    uint32_t timeout = 10000000;
    while (port->cmd & (AHCI_PXCMD_FR | AHCI_PXCMD_CR)) {
        if (--timeout == 0) { kprintf("AHCI: timeout deteniendo puerto\n"); return 0; }
    }
    return 1;
}

static int ahci_start_port(HBA_PORT *port) {
    uint32_t timeout = 10000000;
    while (port->cmd & AHCI_PXCMD_CR) {
        if (--timeout == 0) { kprintf("AHCI: timeout command engine\n"); return 0; }
    }
    port->cmd |= AHCI_PXCMD_FRE;
    port->cmd |= AHCI_PXCMD_ST;
    return 1;
}

static int ahci_setup_port() {
    if (!ahci_port) return 0;
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
    header->dw0 = 5 | (1U << 16);
    header->prdbc = 0;
    header->ctba  = ctba;
    header->ctbau = 0;

    ahci_port->is   = 0xFFFFFFFF;
    ahci_port->serr = 0xFFFFFFFF;
    ahci_port->ie   = 0;

    if (!ahci_start_port(ahci_port)) return 0;

    kprintf("AHCI: puerto preparado CLB=%x FB=%x CTBA=%x\n", clb, fb, ctba);
    return 1;
}

static int ahci_wait_not_busy() {
    if (!ahci_port) return 0;
    uint32_t timeout = 10000000;
    while (ahci_port->tfd & (AHCI_TFD_BSY | AHCI_TFD_DRQ)) {
        if (--timeout == 0) { kprintf("AHCI: timeout TFD\n"); return 0; }
    }
    return 1;
}

static int ahci_issue_command(uint8_t command, uint64_t lba, uint16_t sector_count) {
    if (!ahci_port) return 0;
    if (sector_count == 0) return 0;
    if (lba > 0x0000FFFFFFFFFFFFULL) return 0;
    if (!ahci_wait_not_busy()) return 0;

    if (ahci_port->sact & 1) { kprintf("AHCI: SLOT0 ocupado SACT\n"); return 0; }
    if (ahci_port->ci & 1)   { kprintf("AHCI: SLOT0 ocupado CI\n");   return 0; }

    memzero(&ahci_cmd_table, sizeof(ahci_cmd_table));

    HBA_CMD_HEADER *header = &ahci_cmd_list[0];
    header->dw0 = 5 | (1U << 16);
    header->prdbc = 0;

    HBA_PRDT *prdt = &ahci_cmd_table.prdt[0];
    prdt->dba = (uint32_t)&ahci_dma_buffer[0];
    prdt->dbau = 0;
    prdt->reserved = 0;
    prdt->dbc = (511U & 0x003FFFFFU) | 0x80000000U;

    uint8_t *fis = ahci_cmd_table.cfis;
    memzero(fis, 64);
    fis[0]  = FIS_TYPE_REG_H2D;
    fis[1]  = 0x80;
    fis[2]  = command;
    fis[3]  = 0;
    fis[4]  = (uint8_t)(lba & 0xFF);
    fis[5]  = (uint8_t)((lba >> 8) & 0xFF);
    fis[6]  = (uint8_t)((lba >> 16) & 0xFF);
    fis[7]  = 0x40;
    fis[8]  = (uint8_t)((lba >> 24) & 0xFF);
    fis[9]  = (uint8_t)((lba >> 32) & 0xFF);
    fis[10] = (uint8_t)((lba >> 40) & 0xFF);
    fis[11] = 0;
    fis[12] = (uint8_t)(sector_count & 0xFF);
    fis[13] = (uint8_t)((sector_count >> 8) & 0xFF);

    ahci_port->ci = 1;
    uint32_t timeout = 50000000;
    while (ahci_port->ci & 1) {
        if (ahci_port->is & AHCI_PXIS_TFES) {
            kprintf("AHCI: TASK FILE ERROR IS=%x TFD=%x SERR=%x\n",
                    ahci_port->is, ahci_port->tfd, ahci_port->serr);
            ahci_port->is = 0xFFFFFFFF;
            ahci_port->serr = 0xFFFFFFFF;
            return 0;
        }
        if (--timeout == 0) {
            kprintf("AHCI: timeout CI=%x IS=%x TFD=%x\n",
                    ahci_port->ci, ahci_port->is, ahci_port->tfd);
            return 0;
        }
    }

    if (ahci_port->is & AHCI_PXIS_TFES) {
        kprintf("AHCI: error post-completado\n");
        ahci_port->is = 0xFFFFFFFF;
        ahci_port->serr = 0xFFFFFFFF;
        return 0;
    }

    ahci_port->is = 0xFFFFFFFF;
    ahci_port->serr = 0xFFFFFFFF;
    return 1;
}

static int ahci_identify() {
    if (!ahci_port) return 0;

    memzero(ahci_dma_buffer, sizeof(ahci_dma_buffer));

    HBA_CMD_HEADER *header = &ahci_cmd_list[0];
    header->dw0 = 5 | (1U << 16);
    header->prdbc = 0;

    memzero(&ahci_cmd_table, sizeof(ahci_cmd_table));

    HBA_PRDT *prdt = &ahci_cmd_table.prdt[0];
    prdt->dba = (uint32_t)&ahci_identify_data[0];
    prdt->dbau = 0;
    prdt->reserved = 0;
    prdt->dbc = (511U & 0x003FFFFFU) | 0x80000000U;

    uint8_t *fis = ahci_cmd_table.cfis;
    memzero(fis, 64);
    fis[0] = FIS_TYPE_REG_H2D;
    fis[1] = 0x80;
    fis[2] = ATA_CMD_IDENTIFY;
    fis[7] = 0;

    if (!ahci_wait_not_busy()) return 0;

    ahci_port->ci = 1;
    uint32_t timeout = 50000000;
    while (ahci_port->ci & 1) {
        if (ahci_port->is & AHCI_PXIS_TFES) {
            kprintf("AHCI: IDENTIFY fallo\n");
            ahci_port->is = 0xFFFFFFFF;
            ahci_port->serr = 0xFFFFFFFF;
            return 0;
        }
        if (--timeout == 0) { kprintf("AHCI: timeout IDENTIFY\n"); return 0; }
    }

    if (ahci_port->is & AHCI_PXIS_TFES) {
        kprintf("AHCI: IDENTIFY TASK FILE ERROR\n");
        return 0;
    }

    uint16_t command_sets = ahci_identify_data[83];
    if (command_sets & (1 << 10)) {
        uint64_t lba48 =
            ((uint64_t)ahci_identify_data[103] << 48) |
            ((uint64_t)ahci_identify_data[102] << 32) |
            ((uint64_t)ahci_identify_data[101] << 16) |
            ((uint64_t)ahci_identify_data[100]);
        ahci_sector_count = lba48;
    } else {
        uint32_t lba28 =
            ((uint32_t)ahci_identify_data[61] << 16) |
            ((uint32_t)ahci_identify_data[60]);
        ahci_sector_count = lba28;
    }

    char model[41];
    for (int i = 0; i < 40; i += 2) {
        uint16_t w = ahci_identify_data[27 + (i / 2)];
        model[i]     = (char)(w >> 8);
        model[i + 1] = (char)(w & 0xFF);
    }
    model[40] = 0;
    for (int i = 39; i >= 0; i--) {
        if (model[i] == ' ' || model[i] == '\0') model[i] = '\0';
        else break;
    }

    kprintf("AHCI: IDENTIFY OK\n");
    kprintf("AHCI: modelo: %s\n", model);
    kprintf("AHCI: sectores: %u:%u\n",
            (uint32_t)(ahci_sector_count >> 32),
            (uint32_t)(ahci_sector_count));

    uint32_t mib = (uint32_t)(ahci_sector_count / 2048);
    kprintf("AHCI: capacidad aprox: %u MiB\n", mib);

    ahci_port->is = 0xFFFFFFFF;
    ahci_port->serr = 0xFFFFFFFF;
    return 1;
}

extern "C" int ahci_init() {
    kprintf("\n============================\n");
    kprintf(" AHCI DRIVER\n");
    kprintf("============================\n");

    ahci_ready = 0;

    if (!ahci_find_pci()) {
        kprintf("AHCI: controlador no encontrado\n");
        return 0;
    }

    if (!ahci_enable_pci()) {
        kprintf("AHCI: no se pudo habilitar PCI\n");
        return 0;
    }

    ahci_hba = ahci_get_abar();
    if (!ahci_hba) {
        kprintf("AHCI: ABAR invalido\n");
        return 0;
    }

    kprintf("AHCI: CAP=%x\n", ahci_hba->cap);
    kprintf("AHCI: PI=%x\n",  ahci_hba->pi);
    kprintf("AHCI: VS=%x\n",  ahci_hba->vs);

    ahci_hba->ghc |= AHCI_GHC_AE;
    if ((ahci_hba->ghc & AHCI_GHC_AE) == 0) {
        kprintf("AHCI: no se pudo activar AE\n");
        return 0;
    }

    if (!ahci_find_port()) {
        kprintf("AHCI: no se encontro disco SATA\n");
        return 0;
    }

    if (!ahci_setup_port()) {
        kprintf("AHCI: fallo preparando puerto\n");
        return 0;
    }

    if (!ahci_identify()) {
        kprintf("AHCI: IDENTIFY fallo\n");
        return 0;
    }

    ahci_ready = 1;
    kprintf("AHCI: DRIVER LISTO\n");
    return 1;
}

extern "C" int ahci_read_sector(uint64_t lba, void *buffer) {
    if (!ahci_ready) return 0;
    if (!buffer) return 0;

    if (lba >= ahci_sector_count) {
        kprintf("AHCI: LBA fuera del disco\n");
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

extern "C" uint64_t ahci_get_sector_count() {
    return ahci_sector_count;
}

extern "C" int ahci_is_ready() {
    return ahci_ready;
}
