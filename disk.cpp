// disk.cpp
#include "disk.h"
#include "shell.h"
#include "win.h"
#include "ahci.h"

/* =====================================================================
 * Helpers de impresion (acceso a las funciones del shell)
 *
 * Como shell.cpp tiene term_print y term_println estaticas, necesitamos
 * exponer algo o duplicar aqui. Duplicamos: no queremos tocar shell.cpp
 * demasiado.
 * ===================================================================== */
extern "C" void shell_term_print(int wid, const char *s);
extern "C" void shell_term_println(int wid, const char *s);
extern "C" void shell_print_uint(int wid, unsigned int v);

#define tp(w,s)  shell_term_print((w),(s))
#define tpl(w,s) shell_term_println((w),(s))
#define tpu(w,v) shell_print_uint((w),(v))

/* =====================================================================
 * Estructura del MBR (offset 446, 4 entradas de 16 bytes)
 * ===================================================================== */
struct MbrPart {
    unsigned char boot;
    unsigned char chs_start[3];
    unsigned char type;
    unsigned char chs_end[3];
    unsigned int  lba_start;
    unsigned int  sectors;
} __attribute__((packed));

#define MBR_PART_OFFSET  446
#define MBR_SIG_OFFSET   510

/* Cache del sector 0 */
static unsigned char mbr_buf[512];
static int mbr_loaded = 0;

static int load_mbr(void) {
    if (mbr_loaded) return 1;
    if (!ahci_is_ready()) return 0;
    if (!ahci_read_sector(0, mbr_buf)) return 0;
    mbr_loaded = 1;
    return 1;
}

/* =====================================================================
 * Comando: hex <lba>
 * ===================================================================== */
int disk_cmd_hex(int wid, const char *arg) {
    if (!ahci_is_ready()) {
        tpl(wid, "hex: AHCI no disponible");
        return 1;
    }

    /* Parsear el LBA (decimal) */
    unsigned int lba = 0;
    int i = 0;
    if (!arg || !*arg) {
        tpl(wid, "uso: hex <lba>");
        return 1;
    }
    while (arg[i] >= '0' && arg[i] <= '9') {
        lba = lba * 10 + (unsigned int)(arg[i] - '0');
        i++;
    }
    if (i == 0) {
        tpl(wid, "uso: hex <lba>");
        return 1;
    }

    static unsigned char buf[512];
    if (!ahci_read_sector(lba, buf)) {
        tpl(wid, "hex: fallo al leer");
        return 1;
    }

    tp(wid, "LBA ");
    tpu(wid, lba);
    tpl(wid, ":");

    /* 32 filas x 16 bytes */
    for (int row = 0; row < 32; row++) {
        /* Offset */
        char off[8];
        unsigned int o = (unsigned int)row * 16;
        off[0] = "0123456789ABCDEF"[(o >> 12) & 0xF];
        off[1] = "0123456789ABCDEF"[(o >> 8)  & 0xF];
        off[2] = "0123456789ABCDEF"[(o >> 4)  & 0xF];
        off[3] = "0123456789ABCDEF"[o & 0xF];
        off[4] = ':';
        off[5] = ' ';
        off[6] = 0;
        tp(wid, off);

        /* 16 bytes en hex */
        for (int col = 0; col < 16; col++) {
            unsigned char b = buf[row * 16 + col];
            char hex[3];
            hex[0] = "0123456789ABCDEF"[(b >> 4) & 0xF];
            hex[1] = "0123456789ABCDEF"[b & 0xF];
            hex[2] = ' ';
            win_sys_write(wid, hex, 3);
        }

        /* ASCII */
        tp(wid, " |");
        for (int col = 0; col < 16; col++) {
            unsigned char b = buf[row * 16 + col];
            char ch[2];
            ch[0] = (b >= 32 && b < 127) ? (char)b : '.';
            ch[1] = 0;
            win_sys_write(wid, ch, 1);
        }
        tpl(wid, "|");
    }

    return 1;
}

/* =====================================================================
 * Comando: partitions
 * ===================================================================== */
static const char *part_type_name(unsigned char t) {
    switch (t) {
        case 0x00: return "Vacio";
        case 0x01: return "FAT12";
        case 0x04: return "FAT16";
        case 0x05: return "Extendido";
        case 0x06: return "FAT16B";
        case 0x07: return "NTFS/exFAT";
        case 0x0B: return "FAT32";
        case 0x0C: return "FAT32 LBA";
        case 0x0E: return "FAT16 LBA";
        case 0x0F: return "Extendido LBA";
        case 0x27: return "WinRE";
        case 0x82: return "Linux swap";
        case 0x83: return "Linux";
        case 0x8E: return "LVM";
        case 0xEE: return "GPT (protective)";
        case 0xEF: return "EFI System";
        default:   return "?";
    }
}

int disk_cmd_partitions(int wid) {
    if (!load_mbr()) {
        tpl(wid, "partitions: AHCI no disponible o MBR no leible");
        return 1;
    }

    /* Comprobar firma del MBR */
    if (mbr_buf[MBR_SIG_OFFSET] != 0x55 || mbr_buf[MBR_SIG_OFFSET + 1] != 0xAA) {
        tpl(wid, "partitions: firma MBR 0x55AA no encontrada");
        return 1;
    }

    tpl(wid, "=== PARTICIONES (MBR) ===");
    tpl(wid, "#  Tipo            Inicio LBA    Sectores       Tamano");

    int found = 0;
    for (int i = 0; i < 4; i++) {
        struct MbrPart *p = (struct MbrPart *)(mbr_buf + MBR_PART_OFFSET + i * 16);

        if (p->type == 0) continue;

        /* Formatear la linea */
        char line[80];
        int k = 0;

        /* Numero */
        line[k++] = '0' + i;
        line[k++] = ' ';
        line[k++] = ' ';

        /* Tipo (0xXX) */
        line[k++] = '0'; line[k++] = 'x';
        line[k++] = "0123456789ABCDEF"[(p->type >> 4) & 0xF];
        line[k++] = "0123456789ABCDEF"[p->type & 0xF];
        line[k++] = ' ';
        line[k++] = ' ';

        /* Nombre tipo */
        const char *name = part_type_name(p->type);
        for (int j = 0; name[j] && k < 24; j++) line[k++] = name[j];
        while (k < 24) line[k++] = ' ';

        /* LBA start */
        unsigned int v = p->lba_start;
        char tmp[12]; int t = 0;
        if (v == 0) tmp[t++] = '0';
        while (v) { tmp[t++] = '0' + (v % 10); v /= 10; }
        while (t > 0 && k < 38) line[k++] = tmp[--t];
        while (k < 38) line[k++] = ' ';

        /* Sectores */
        v = p->sectors;
        t = 0;
        if (v == 0) tmp[t++] = '0';
        while (v) { tmp[t++] = '0' + (v % 10); v /= 10; }
        while (t > 0 && k < 52) line[k++] = tmp[--t];
        while (k < 52) line[k++] = ' ';

        /* Tamano en MiB */
        unsigned int mib = (p->sectors / 2048);   /* 1 MiB = 2048 sectores de 512 */
        v = mib;
        t = 0;
        if (v == 0) tmp[t++] = '0';
        while (v) { tmp[t++] = '0' + (v % 10); v /= 10; }
        while (t > 0 && k < 64) line[k++] = tmp[--t];
        const char *suf = " MiB";
        for (int j = 0; suf[j] && k < 79; j++) line[k++] = suf[j];
        line[k] = 0;

        tpl(wid, line);
        found++;
    }

    if (found == 0) {
        tpl(wid, "(no hay particiones validas)");
    }

    return 1;
}

/* =====================================================================
 * Comando: disk (el que ya tenias, movido aqui)
 * ===================================================================== */
int disk_cmd_disk(int wid) {
    tpl(wid, "=== DISCO AHCI ===");

    if (!ahci_is_ready()) {
        tpl(wid, "AHCI: no disponible");
        return 1;
    }

    unsigned int count = (unsigned int)ahci_get_sector_count();
    unsigned int mb = (unsigned int)((ahci_get_sector_count() * 512ULL) / (1024ULL * 1024ULL));

    tpl(wid, "Estado:    LISTO");
    tp(wid, "Sectores:  "); tpu(wid, count); tpl(wid, "");
    tp(wid, "Capacidad: "); tpu(wid, mb); tpl(wid, " MiB");

    static unsigned char buf[512];
    if (!ahci_read_sector(0, buf)) {
        tpl(wid, "Error al leer sector 0");
        return 1;
    }

    tpl(wid, "MBR (primeros 16 bytes):");
    for (int i = 0; i < 16; i++) {
        char hex[4];
        unsigned char b = buf[i];
        hex[0] = "0123456789ABCDEF"[(b >> 4) & 0xF];
        hex[1] = "0123456789ABCDEF"[b & 0xF];
        hex[2] = ' ';
        hex[3] = 0;
        tp(wid, hex);
    }
    tpl(wid, "");

    if (buf[510] == 0x55 && buf[511] == 0xAA) {
        tpl(wid, "Firma 0x55AA: OK (hay MBR)");
    } else {
        tpl(wid, "Firma 0x55AA: NO (disco sin MBR)");
    }

    return 1;
}

/* =====================================================================
 * Init
 * ===================================================================== */
void disk_init(void) {
    mbr_loaded = 0;
}
