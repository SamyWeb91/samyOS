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

// ahci.cpp
// Driver AHCI mínimo en modo SOLO LECTURA.
// Comandos soportados: IDENTIFY DEVICE (0xEC) y READ DMA EXT (0x25).
// Ningún comando de escritura está disponible.

#include <stdint.h>
#include "screen.h"
#include "io.h"
#include "paging.h"

/* =====================================================================
 * PCI
 * ===================================================================== */
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

#define AHCI_CLASS          0x01
#define AHCI_SUBCLASS       0x06
#define AHCI_PROGIF         0x01

/* =====================================================================
 * Log interno para diagnóstico (lo lee el comando `ahcilog` del shell)
 * ===================================================================== */
#define AHCI_LOG_MAX  32
#define AHCI_LOG_LINE 80
static char ahci_log[AHCI_LOG_MAX][AHCI_LOG_LINE];
static int  ahci_log_count = 0;

static void ahci_log_add(const char *s) {
    if (ahci_log_count >= AHCI_LOG_MAX) return;
    int i = 0;
    while (s[i] && i < AHCI_LOG_LINE - 1) {
        ahci_log[ahci_log_count][i] = s[i];
        i++;
    }
    ahci_log[ahci_log_count][i] = 0;
    ahci_log_count++;
}

static void ahci_log_reset(void) {
    ahci_log_count = 0;
}

extern "C" int         ahci_log_count_get(void) { return ahci_log_count; }
extern "C" const char *ahci_log_get(int n) {
    if (n < 0 || n >= ahci_log_count) return 0;
    return ahci_log[n];
}

/* =====================================================================
 * PCI config space
 * ===================================================================== */
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
    /* FIX #6: proteger el par CF8/CFC contra interrupciones */
    asm volatile("cli");
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
    asm volatile("sti");
}

/* =====================================================================
 * Constantes AHCI
 * ===================================================================== */
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
 * Memoria DMA
 * ===================================================================== */
static HBA_CMD_HEADER ahci_cmd_list[32]        __attribute__((aligned(1024)));
static uint8_t        ahci_received_fis[256]   __attribute__((aligned(256)));
static HBA_CMD_TABLE  ahci_cmd_table           __attribute__((aligned(128)));
static uint8_t        ahci_dma_buffer[512]     __attribute__((aligned(512)));
static uint16_t       ahci_identify_data[256]  __attribute__((aligned(512)));

/* =====================================================================
 * Estado
 * ===================================================================== */
static HBA_MEM  *ahci_hba  = 0;
static HBA_PORT *ahci_port = 0;
static uint8_t   ahci_pci_bus  = 0;
static uint8_t   ahci_pci_dev  = 0;
static uint8_t   ahci_pci_func = 0;
static uint64_t  ahci_sector_count = 0;
static int       ahci_ready = 0;
static int       ahci_port_index = -1;   /* FIX #4: guardar índice del puerto */

/* =====================================================================
 * Helpers
 * ===================================================================== */
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

/* FIX #5: delay corto (settle) */
static void ahci_delay(volatile uint32_t loops)
{
    for (volatile uint32_t i = 0; i < loops; i++) {
        asm volatile("pause");
    }
}

/* Helpers de formato mini (sin sprintf) para el log */
static void log_hex(uint32_t v, char *out) {
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++) out[i] = hex[(v >> ((7 - i) * 4)) & 0xF];
    out[8] = 0;
}

static void log_dec(uint32_t v, char *out) {
    char tmp[12]; int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
    int k = 0;
    for (int i = n - 1; i >= 0; i--) out[k++] = tmp[i];
    out[k] = 0;
}

/* FIX #3: helper para añadir texto al log */
static void log_append(char *buf, int *k, const char *s) {
    while (*s && *k < AHCI_LOG_LINE - 1) buf[(*k)++] = *s++;
}
static void log_append_hex(char *buf, int *k, uint32_t v) {
    char h[12]; log_hex(v, h);
    for (int i = 0; h[i] && *k < AHCI_LOG_LINE - 1; i++) buf[(*k)++] = h[i];
}
static void log_append_dec(char *buf, int *k, uint32_t v) {
    char d[12]; log_dec(v, d);
    for (int i = 0; d[i] && *k < AHCI_LOG_LINE - 1; i++) buf[(*k)++] = d[i];
}
static void log_append_hex4(char *buf, int *k, uint32_t v) {
    /* solo 4 nibbles bajos, sin padding */
    static const char hex[] = "0123456789abcdef";
    char tmp[8]; int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = hex[v & 0xF]; v >>= 4; }
    for (int i = n - 1; i >= 0 && *k < AHCI_LOG_LINE - 1; i--) buf[(*k)++] = tmp[i];
}

/* =====================================================================
 * Búsqueda del controlador en el bus PCI
 * ===================================================================== */
static int ahci_find_pci(void)
{
    /* FIX #1: limitar al bus 0 (el 99% de laptops tienen el SATA ahí).
       Si quisieras escanear más, primero enumera buses con un bridge. */
    for (uint32_t bus = 0; bus < 1; bus++)
    for (uint32_t dev = 0; dev < 32; dev++)
    for (uint32_t func = 0; func < 8; func++) {
        uint32_t id = pci_read32((uint8_t)bus, (uint8_t)dev, (uint8_t)func, 0x00);

        /* FIX #2: filtrar 0xFFFF y 0x0000 */
        if ((id & 0xFFFF) == 0xFFFF) continue;
        if (id == 0)                 continue;

        uint32_t class_reg = pci_read32((uint8_t)bus, (uint8_t)dev, (uint8_t)func, 0x08);
        uint8_t prog_if    = (class_reg >> 8)  & 0xFF;
        uint8_t subclass   = (class_reg >> 16) & 0xFF;
        uint8_t class_code = (class_reg >> 24) & 0xFF;

        if (class_code == AHCI_CLASS && subclass == AHCI_SUBCLASS && prog_if == AHCI_PROGIF) {
            ahci_pci_bus  = (uint8_t)bus;
            ahci_pci_dev  = (uint8_t)dev;
            ahci_pci_func = (uint8_t)func;

            char buf[AHCI_LOG_LINE]; int k = 0;
            log_append(buf, &k, "AHCI: PCI ");
            log_append_hex4(buf, &k, bus);
            log_append(buf, &k, ":");
            log_append_hex4(buf, &k, dev);
            log_append(buf, &k, ".");
            log_append_hex4(buf, &k, func);
            log_append(buf, &k, " vendor=");
            log_append_hex4(buf, &k, id & 0xFFFF);
            log_append(buf, &k, " device=");
            log_append_hex4(buf, &k, id >> 16);
            buf[k] = 0;
            ahci_log_add(buf);

            kprintf("AHCI: PCI %x:%x.%x vendor=%x device=%x\n",
                    bus, dev, func, id & 0xFFFF, id >> 16);
            return 1;
        }
    }
    return 0;
}

static int ahci_enable_pci(void)
{
    uint16_t command = pci_read16(ahci_pci_bus, ahci_pci_dev, ahci_pci_func, 0x04);
    command |= (1 << 1) | (1 << 2);   /* Memory Space + Bus Master */
    pci_write16(ahci_pci_bus, ahci_pci_dev, ahci_pci_func, 0x04, command);

    /* FIX #7: esperar un poco a que el chipset aplique los cambios */
    ahci_delay(10000);

    command = pci_read16(ahci_pci_bus, ahci_pci_dev, ahci_pci_func, 0x04);
    if ((command & (1 << 1)) == 0 || (command & (1 << 2)) == 0) {
        ahci_log_add("AHCI: PCI enable fail");
        kprintf("AHCI: fallo habilitando PCI (CMD=%x)\n", command);
        return 0;
    }
    return 1;
}

static HBA_MEM *ahci_get_abar(void)
{
    uint32_t bar5 = pci_read32(ahci_pci_bus, ahci_pci_dev, ahci_pci_func, 0x24);

    /* FIX: si BAR5 = 0 y es 64-bit, comprobar type */
    if ((bar5 & 1) != 0) {
        ahci_log_add("AHCI: BAR5 es I/O, no MMIO");
        kprintf("AHCI: BAR5 es I/O, no MMIO\n");
        return 0;
    }

    uint32_t type = (bar5 >> 1) & 0x3;
    uint32_t abar = bar5 & 0xFFFFFFF0U;

    if (type == 0x2) {
        /* BAR 64-bit: el high está en offset 0x28 */
        uint32_t bar6 = pci_read32(ahci_pci_bus, ahci_pci_dev, ahci_pci_func, 0x28);
        kprintf("AHCI: BAR5 es 64-bit (high=%x)\n", bar6);
        /* En x86-32 asumimos high=0 */
    }

    if (!abar) {
        ahci_log_add("AHCI: ABAR = 0");
        kprintf("AHCI: ABAR = 0\n");
        return 0;
    }

    {
        char buf[AHCI_LOG_LINE]; int k = 0;
        log_append(buf, &k, "AHCI: ABAR = ");
        log_append_hex(buf, &k, abar);
        buf[k] = 0;
        ahci_log_add(buf);
    }
    kprintf("AHCI: ABAR = %x\n", abar);

    paging_map_range(abar, abar, 32, PTE_PRESENT | PTE_RW | PTE_PCD | PTE_PWT);

    /* FIX: verificar que el mapeo funciona leyendo el CAP */
    volatile uint32_t *cap_ptr = (volatile uint32_t *)abar;
    uint32_t cap_test = *cap_ptr;
    kprintf("AHCI: CAP (post-map) = %x\n", cap_test);

    if (cap_test == 0xFFFFFFFF || cap_test == 0) {
        ahci_log_add("AHCI: ABAR no responde");
        kprintf("AHCI: ABAR no responde (CAP=%x)\n", cap_test);
        return 0;
    }

    ahci_log_add("AHCI: ABAR mapeado (32 KB, uncached)");
    kprintf("AHCI: ABAR mapeado (32 KB, uncached)\n");
    return (HBA_MEM *)abar;
}

/* =====================================================================
 * Puerto SATA
 * ===================================================================== */
static int ahci_port_is_sata(HBA_PORT *port)
{
    uint32_t ssts = port->ssts;
    uint8_t det = ssts & 0x0F;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    if (det != SATA_DET_PRESENT || ipm != SATA_IPM_ACTIVE) return 0;
    if (port->sig == SATA_SIG_ATAPI) return 0;
    return 1;
}

/* FIX #4: esperar a que el puerto negocie con el disco.
   En hardware real, tras activar GHC.AE, DET pasa por 1 (present-no-comm)
   antes de llegar a 3 (present). Si lees inmediatamente, fallas. */
static int ahci_wait_port_ready(HBA_PORT *port)
{
    for (uint32_t i = 0; i < 500000; i++) {
        uint32_t ssts = port->ssts;
        uint8_t det = ssts & 0x0F;
        uint8_t ipm = (ssts >> 8) & 0x0F;
        if (det == SATA_DET_PRESENT && ipm == SATA_IPM_ACTIVE) return 1;
        ahci_delay(50);
    }
    return 0;
}

static int ahci_find_port(void)
{
    uint32_t pi = ahci_hba->pi;
    kprintf("AHCI: PI = %x (puertos implementados)\n", pi);

    for (int i = 0; i < 32; i++) {
        if ((pi & (1U << i)) == 0) continue;
        HBA_PORT *port = &ahci_hba->ports[i];

        kprintf("AHCI: puerto %d esperando negociacion...\n", i);

        if (!ahci_wait_port_ready(port)) {
            char buf[AHCI_LOG_LINE]; int k = 0;
            log_append(buf, &k, "AHCI: puerto ");
            log_append_dec(buf, &k, (uint32_t)i);
            log_append(buf, &k, " no listo DET=");
            log_append_hex4(buf, &k, port->ssts & 0xF);
            buf[k] = 0;
            ahci_log_add(buf);

            kprintf("AHCI: puerto %d no listo (DET=%x IPM=%x)\n",
                    i, port->ssts & 0xF, (port->ssts >> 8) & 0xF);
            continue;
        }

        {
            char buf[AHCI_LOG_LINE]; int k = 0;
            log_append(buf, &k, "AHCI: puerto ");
            log_append_dec(buf, &k, (uint32_t)i);
            log_append(buf, &k, " DET=");
            log_append_hex4(buf, &k, port->ssts & 0xF);
            log_append(buf, &k, " IPM=");
            log_append_hex4(buf, &k, (port->ssts >> 8) & 0xF);
            log_append(buf, &k, " SIG=");
            log_append_hex(buf, &k, port->sig);
            buf[k] = 0;
            ahci_log_add(buf);
        }
        kprintf("AHCI: puerto %d DET=%x IPM=%x SIG=%x\n",
                i, port->ssts & 0xF, (port->ssts >> 8) & 0xF, port->sig);

        if (ahci_port_is_sata(port)) {
            ahci_port = port;
            ahci_port_index = i;

            char buf[AHCI_LOG_LINE]; int k = 0;
            log_append(buf, &k, "AHCI: disco SATA en puerto ");
            log_append_dec(buf, &k, (uint32_t)i);
            buf[k] = 0;
            ahci_log_add(buf);

            kprintf("AHCI: disco SATA en puerto %d\n", i);
            return 1;
        }
    }
    return 0;
}

static int ahci_wait_clear(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t t = 10000000;
    while ((*reg & mask) != 0) {
        if (--t == 0) return 0;
        ahci_delay(10);
    }
    return 1;
}

static int ahci_stop_port(HBA_PORT *port)
{
    port->cmd &= ~AHCI_PXCMD_ST;
    if (!ahci_wait_clear(&port->cmd, AHCI_PXCMD_CR)) {
        ahci_log_add("AHCI: timeout CR=0");
        kprintf("AHCI: timeout CR=0 (CMD=%x)\n", port->cmd);
        return 0;
    }
    port->cmd &= ~AHCI_PXCMD_FRE;
    uint32_t t = 10000000;
    while (port->cmd & (AHCI_PXCMD_FR | AHCI_PXCMD_CR)) {
        if (--t == 0) {
            ahci_log_add("AHCI: timeout stop puerto");
            kprintf("AHCI: timeout stop puerto (CMD=%x)\n", port->cmd);
            return 0;
        }
        ahci_delay(10);
    }
    return 1;
}

static int ahci_start_port(HBA_PORT *port)
{
    uint32_t t = 10000000;
    while (port->cmd & AHCI_PXCMD_CR) {
        if (--t == 0) {
            ahci_log_add("AHCI: timeout command engine");
            kprintf("AHCI: timeout command engine\n");
            return 0;
        }
        ahci_delay(10);
    }
    port->cmd |= AHCI_PXCMD_FRE;
    port->cmd |= AHCI_PXCMD_ST;
    return 1;
}

static int ahci_setup_port(void)
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

    ahci_log_add("AHCI: puerto listo");
    kprintf("AHCI: puerto listo CLB=%x FB=%x CTBA=%x\n", clb, fb, ctba);
    return 1;
}

static int ahci_wait_not_busy(void)
{
    uint32_t t = 10000000;
    while (ahci_port->tfd & (AHCI_TFD_BSY | AHCI_TFD_DRQ)) {
        if (--t == 0) {
            ahci_log_add("AHCI: timeout TFD BSY/DRQ");
            kprintf("AHCI: timeout TFD BSY/DRQ (TFD=%x)\n", ahci_port->tfd);
            return 0;
        }
        ahci_delay(10);
    }
    return 1;
}

/* =====================================================================
 * Emitir comando de lectura (SOLO LECTURA)
 * ===================================================================== */
static int ahci_issue_read(uint64_t lba, uint32_t byte_count)
{
    if (!ahci_port) return 0;
    if (byte_count == 0 || byte_count > 512) return 0;

    if (!ahci_wait_not_busy()) return 0;

    {
        uint32_t t = 10000000;
        while ((ahci_port->ci & 1) || (ahci_port->sact & 1)) {
            if (--t == 0) {
                ahci_log_add("AHCI: slot ocupado");
                kprintf("AHCI: slot ocupado CI=%x SACT=%x\n",
                        ahci_port->ci, ahci_port->sact);
                return 0;
            }
            ahci_delay(10);
        }
    }

    ahci_port->is   = 0xFFFFFFFF;
    ahci_port->serr = 0xFFFFFFFF;

    memzero(&ahci_cmd_table, sizeof(ahci_cmd_table));

    HBA_CMD_HEADER *header = &ahci_cmd_list[0];
    header->dw0   = 5 | (1U << 16);
    header->prdbc = 0;
    header->ctba  = (uint32_t)&ahci_cmd_table;
    header->ctbau = 0;

    HBA_PRDT *prdt = &ahci_cmd_table.prdt[0];
    prdt->dba      = (uint32_t)&ahci_dma_buffer[0];
    prdt->dbau     = 0;
    prdt->reserved = 0;
    prdt->dbc      = (byte_count - 1) & 0x003FFFFFU;

    uint8_t *fis = ahci_cmd_table.cfis;
    memzero(fis, 64);
    fis[0]  = FIS_TYPE_REG_H2D;
    fis[1]  = 0x80;
    fis[2]  = ATA_CMD_READ_DMA_EXT;
    fis[3]  = 0;
    fis[4]  = (uint8_t)(lba);
    fis[5]  = (uint8_t)(lba >> 8);
    fis[6]  = (uint8_t)(lba >> 16);
    fis[7]  = 0x40;
    fis[8]  = (uint8_t)(lba >> 24);
    fis[9]  = (uint8_t)(lba >> 32);
    fis[10] = (uint8_t)(lba >> 40);
    fis[11] = 0;
    fis[12] = 1;
    fis[13] = 0;

    ahci_port->ci = 1;

    uint32_t timeout = 50000000;
    while (ahci_port->ci & 1) {
        if (ahci_port->is & AHCI_PXIS_TFES) {
            ahci_log_add("AHCI: error de lectura (TFES)");
            kprintf("AHCI: error de lectura IS=%x TFD=%x SERR=%x\n",
                    ahci_port->is, ahci_port->tfd, ahci_port->serr);
            ahci_port->is   = 0xFFFFFFFF;
            ahci_port->serr = 0xFFFFFFFF;
            return 0;
        }
        if (--timeout == 0) {
            ahci_log_add("AHCI: timeout lectura");
            kprintf("AHCI: timeout lectura CI=%x IS=%x TFD=%x\n",
                    ahci_port->ci, ahci_port->is, ahci_port->tfd);
            return 0;
        }
        ahci_delay(10);
    }

    if (!ahci_wait_not_busy()) return 0;

    if (ahci_port->is & AHCI_PXIS_TFES) {
        ahci_log_add("AHCI: error post-lectura (TFES)");
        kprintf("AHCI: error post-lectura IS=%x TFD=%x\n",
                ahci_port->is, ahci_port->tfd);
        ahci_port->is   = 0xFFFFFFFF;
        ahci_port->serr = 0xFFFFFFFF;
        return 0;
    }

    ahci_port->is   = 0xFFFFFFFF;
    ahci_port->serr = 0xFFFFFFFF;
    return 1;
}

/* =====================================================================
 * IDENTIFY DEVICE
 * ===================================================================== */
static int ahci_identify(void)
{
    memzero(ahci_identify_data, sizeof(ahci_identify_data));
    memzero(ahci_dma_buffer, sizeof(ahci_dma_buffer));
    memzero(&ahci_cmd_table, sizeof(ahci_cmd_table));

    HBA_CMD_HEADER *header = &ahci_cmd_list[0];
    header->dw0   = 5 | (1U << 16);
    header->prdbc = 0;
    header->ctba  = (uint32_t)&ahci_cmd_table;
    header->ctbau = 0;

    HBA_PRDT *prdt = &ahci_cmd_table.prdt[0];
    prdt->dba  = (uint32_t)&ahci_identify_data[0];
    prdt->dbau = 0;
    prdt->dbc  = 511 & 0x003FFFFFU;

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
            ahci_log_add("AHCI: IDENTIFY error TFES");
            kprintf("AHCI: IDENTIFY error TFES IS=%x TFD=%x SERR=%x\n",
                    ahci_port->is, ahci_port->tfd, ahci_port->serr);
            return 0;
        }
        if (--timeout == 0) {
            ahci_log_add("AHCI: timeout IDENTIFY");
            kprintf("AHCI: timeout IDENTIFY CI=%x IS=%x TFD=%x\n",
                    ahci_port->ci, ahci_port->is, ahci_port->tfd);
            return 0;
        }
        ahci_delay(10);
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

    {
        char buf[AHCI_LOG_LINE]; int k = 0;
        log_append(buf, &k, "AHCI: modelo='");
        for (int i = 0; model[i] && k < AHCI_LOG_LINE - 20; i++) buf[k++] = model[i];
        log_append(buf, &k, "' sectores=");
        log_append_dec(buf, &k, (uint32_t)ahci_sector_count);
        buf[k] = 0;
        ahci_log_add(buf);
    }
    kprintf("AHCI: modelo='%s' sectores=%u\n",
            model, (uint32_t)ahci_sector_count);

    {
        char buf[AHCI_LOG_LINE]; int k = 0;
        log_append(buf, &k, "AHCI: capacidad ~");
        log_append_dec(buf, &k,
            (uint32_t)((ahci_sector_count * 512ULL) / (1024ULL * 1024ULL)));
        log_append(buf, &k, " MiB");
        buf[k] = 0;
        ahci_log_add(buf);
    }
    kprintf("AHCI: capacidad ~%u MiB\n",
            (uint32_t)((ahci_sector_count * 512ULL) / (1024ULL * 1024ULL)));

    if (ahci_sector_count == 0) return 0;

    ahci_port->is   = 0xFFFFFFFF;
    ahci_port->serr = 0xFFFFFFFF;
    return 1;
}

/* =====================================================================
 * API pública
 * ===================================================================== */
extern "C" int ahci_init(void)
{
    ahci_log_reset();
    ahci_log_add("AHCI: init");

    kprintf("\n=== AHCI (solo lectura) ===\n");
    ahci_ready = 0;
    ahci_sector_count = 0;
    ahci_port_index = -1;

    if (!ahci_find_pci()) {
        ahci_log_add("AHCI: no PCI");
        kprintf("AHCI: no PCI (revisa BIOS: SATA=AHCI)\n");
        return 0;
    }
    if (!ahci_enable_pci()) {
        kprintf("AHCI: PCI enable fail\n");
        return 0;
    }

    ahci_hba = ahci_get_abar();
    if (!ahci_hba) {
        ahci_log_add("AHCI: ABAR fail");
        kprintf("AHCI: ABAR fail\n");
        return 0;
    }

    {
        char buf[AHCI_LOG_LINE]; int k = 0;
        log_append(buf, &k, "AHCI: CAP=");
        log_append_hex(buf, &k, ahci_hba->cap);
        log_append(buf, &k, " PI=");
        log_append_hex(buf, &k, ahci_hba->pi);
        log_append(buf, &k, " VS=");
        log_append_hex(buf, &k, ahci_hba->vs);
        buf[k] = 0;
        ahci_log_add(buf);
    }
    kprintf("AHCI: CAP=%x PI=%x VS=%x\n",
            ahci_hba->cap, ahci_hba->pi, ahci_hba->vs);

    /* Activar el HBA (AE). Esperar settle. */
    ahci_hba->ghc |= AHCI_GHC_AE;
    ahci_delay(10000);

    if ((ahci_hba->ghc & AHCI_GHC_AE) == 0) {
        ahci_log_add("AHCI: no AE");
        kprintf("AHCI: no AE (GHC=%x)\n", ahci_hba->ghc);
        return 0;
    }

    if (!ahci_find_port()) {
        ahci_log_add("AHCI: no SATA");
        kprintf("AHCI: no SATA\n");
        return 0;
    }
    if (!ahci_setup_port()) {
        ahci_log_add("AHCI: setup port fail");
        kprintf("AHCI: setup port fail\n");
        return 0;
    }
    if (!ahci_identify()) {
        ahci_log_add("AHCI: IDENTIFY fail");
        kprintf("AHCI: IDENTIFY fail\n");
        return 0;
    }

    ahci_ready = 1;
    ahci_log_add("AHCI: LISTO");
    kprintf("AHCI: LISTO (solo lectura)\n");
    return 1;
}

extern "C" int ahci_read_sector(uint64_t lba, void *buffer)
{
    if (!ahci_ready) { kprintf("AHCI: no ready\n"); return 0; }
    if (!buffer)     { kprintf("AHCI: buffer NULL\n"); return 0; }
    if (lba >= ahci_sector_count) {
        kprintf("AHCI: LBA %u fuera de rango (max %u)\n",
                (uint32_t)lba, (uint32_t)ahci_sector_count);
        return 0;
    }

    memzero(ahci_dma_buffer, 512);
    if (!ahci_issue_read(lba, 512)) {
        kprintf("AHCI: lectura fallida LBA=%u\n", (uint32_t)lba);
        return 0;
    }

    memcpy8(buffer, ahci_dma_buffer, 512);
    return 1;
}

extern "C" uint64_t ahci_get_sector_count(void) { return ahci_sector_count; }
extern "C" int      ahci_is_ready(void)         { return ahci_ready; }
extern "C" int      ahci_get_port_index(void)   { return ahci_port_index; }
