// mfs.cpp
#include "mfs.h"
#include "ramdisk.h"

/* =====================================================================
 * Formato del mini-FS
 *
 * [0x00] Cabecera:   magic(4) + file_count(4) + reserved(8)
 * [0x10] Tabla:      16 entradas de 32 bytes
 * [0x210] Datos:     el resto del ramdisk
 *
 * Entrada (32 bytes):
 *   [0x00] used:     uint8
 *   [0x01] name[12]: nombre + '\0'
 *   [0x0D] pad[3]
 *   [0x10] offset:   uint32  (relativo a FS_DATA_START)
 *   [0x14] size:     uint32
 *   [0x18] pad2[8]
 * ===================================================================== */
#define FS_MAGIC          0x5346524Du
#define FS_HEADER_SIZE    0x10
#define FS_ENTRY_SIZE     32
#define FS_TABLE_SIZE     (MFS_MAX_FILES * FS_ENTRY_SIZE)
#define FS_DATA_START     (FS_HEADER_SIZE + FS_TABLE_SIZE)

typedef struct {
    unsigned int magic;
    unsigned int file_count;
    unsigned int reserved0;
    unsigned int reserved1;
} FsHeader;

typedef struct {
    unsigned char used;
    char          name[12];
    unsigned char pad[3];
    unsigned int  offset;
    unsigned int  size;
    unsigned char pad2[8];
} FsEntry;

static int mfs_is_mounted = 0;

/* =====================================================================
 * Helpers
 * ===================================================================== */
static void mem_zero(void *p, unsigned int n) {
    unsigned char *b = (unsigned char *)p;
    for (unsigned int i = 0; i < n; i++) b[i] = 0;
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static unsigned int str_len(const char *s) {
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

/* =====================================================================
 * Acceso a disco (ramdisk)
 * ===================================================================== */
static int fs_read_header(FsHeader *h) {
    if (ramdisk_read(MFS_RD_ID, 0, h, sizeof(FsHeader)) != (int)sizeof(FsHeader))
        return -1;
    return 0;
}

static int fs_write_header(const FsHeader *h) {
    if (ramdisk_write(MFS_RD_ID, 0, h, sizeof(FsHeader)) != (int)sizeof(FsHeader))
        return -1;
    return 0;
}

static int fs_read_entry(int i, FsEntry *e) {
    unsigned int off = FS_HEADER_SIZE + i * FS_ENTRY_SIZE;
    if (ramdisk_read(MFS_RD_ID, off, e, sizeof(FsEntry)) != (int)sizeof(FsEntry))
        return -1;
    return 0;
}

static int fs_write_entry(int i, const FsEntry *e) {
    unsigned int off = FS_HEADER_SIZE + i * FS_ENTRY_SIZE;
    if (ramdisk_write(MFS_RD_ID, off, e, sizeof(FsEntry)) != (int)sizeof(FsEntry))
        return -1;
    return 0;
}

/* =====================================================================
 * Operaciones internas
 * ===================================================================== */
static int fs_find(const char *name) {
    for (int i = 0; i < MFS_MAX_FILES; i++) {
        FsEntry e;
        if (fs_read_entry(i, &e) != 0) return -1;
        if (!e.used) continue;
        if (str_eq(e.name, name)) return i;
    }
    return -1;
}

/* =====================================================================
 * API publica
 * ===================================================================== */
extern "C" int mfs_mount(void) {
    if (!ramdisk_exists(MFS_RD_ID)) return -1;

    FsHeader h;
    fs_read_header(&h);

    if (h.magic != FS_MAGIC) {
        /* Formatear */
        h.magic = FS_MAGIC;
        h.file_count = 0;
        h.reserved0 = 0;
        h.reserved1 = 0;
        if (fs_write_header(&h) != 0) return -1;

        FsEntry zero;
        mem_zero(&zero, sizeof(zero));
        for (int i = 0; i < MFS_MAX_FILES; i++) {
            fs_write_entry(i, &zero);
        }
    }

    mfs_is_mounted = 1;
    return 0;
}

extern "C" int mfs_mounted(void) {
    return mfs_is_mounted;
}

extern "C" int mfs_create(const char *name) {
    if (!mfs_is_mounted) return -1;
    if (!name || !*name) return -1;
    if (str_len(name) > 11) return -1;
    if (fs_find(name) >= 0) return -2;   /* ya existe */

    for (int i = 0; i < MFS_MAX_FILES; i++) {
        FsEntry e;
        if (fs_read_entry(i, &e) != 0) return -1;
        if (!e.used) {
            unsigned int data_off = 0;
            for (int j = 0; j < MFS_MAX_FILES; j++) {
                FsEntry o;
                if (fs_read_entry(j, &o) != 0) return -1;
                if (o.used) {
                    unsigned int end = o.offset + o.size;
                    if (end > data_off) data_off = end;
                }
            }
            if (FS_DATA_START + data_off >= ramdisk_size(MFS_RD_ID))
                return -3;   /* sin espacio */

            mem_zero(&e, sizeof(e));
            e.used = 1;
            int k = 0;
            while (name[k] && k < 11) { e.name[k] = name[k]; k++; }
            e.name[k] = 0;
            e.offset = data_off;
            e.size = 0;

            if (fs_write_entry(i, &e) != 0) return -1;

            FsHeader h;
            fs_read_header(&h);
            h.file_count++;
            fs_write_header(&h);
            return 0;
        }
    }
    return -4;   /* tabla llena */
}

extern "C" int mfs_delete(const char *name) {
    if (!mfs_is_mounted) return -1;
    if (!name || !*name) return -1;

    int idx = fs_find(name);
    if (idx < 0) return -1;

    FsEntry e;
    fs_read_entry(idx, &e);
    mem_zero(&e, sizeof(FsEntry));
    fs_write_entry(idx, &e);

    FsHeader h;
    fs_read_header(&h);
    if (h.file_count > 0) h.file_count--;
    fs_write_header(&h);
    return 0;
}

extern "C" int mfs_read_file(const char *name, char *buf, unsigned int max) {
    if (!mfs_is_mounted) return -1;
    if (!name || !buf) return -1;

    int idx = fs_find(name);
    if (idx < 0) return -1;

    FsEntry e;
    fs_read_entry(idx, &e);

    unsigned int n = e.size;
    if (n > max) n = max;

    if (ramdisk_read(MFS_RD_ID, FS_DATA_START + e.offset, buf, n) != (int)n)
        return -1;
    return (int)n;
}

extern "C" int mfs_write_file(const char *name, const char *data, unsigned int len) {
    if (!mfs_is_mounted) return -1;
    if (!name || !data) return -1;

    int idx = fs_find(name);
    if (idx < 0) {
        /* Si no existe, lo creamos */
        if (mfs_create(name) != 0) return -1;
        idx = fs_find(name);
        if (idx < 0) return -1;
    }

    FsEntry e;
    fs_read_entry(idx, &e);

    unsigned int max = ramdisk_size(MFS_RD_ID) - FS_DATA_START - e.offset;
    if (len > max) len = max;

    if (ramdisk_write(MFS_RD_ID, FS_DATA_START + e.offset, data, len) != (int)len)
        return -1;

    e.size = len;
    fs_write_entry(idx, &e);
    return (int)len;
}

extern "C" int mfs_exists(const char *name) {
    if (!mfs_is_mounted) return 0;
    if (!name || !*name) return 0;
    return fs_find(name) >= 0;
}

extern "C" int mfs_count(void) {
    if (!mfs_is_mounted) return 0;
    int count = 0;
    for (int i = 0; i < MFS_MAX_FILES; i++) {
        FsEntry e;
        if (fs_read_entry(i, &e) != 0) continue;
        if (e.used) count++;
    }
    return count;
}

extern "C" int mfs_get(int n, char *name_out, unsigned int *size_out) {
    if (!mfs_is_mounted) return -1;
    if (n < 0 || !name_out) return -1;

    int count = 0;
    for (int i = 0; i < MFS_MAX_FILES; i++) {
        FsEntry e;
        if (fs_read_entry(i, &e) != 0) continue;
        if (!e.used) continue;
        if (count == n) {
            int k = 0;
            while (e.name[k] && k < 11) { name_out[k] = e.name[k]; k++; }
            name_out[k] = 0;
            if (size_out) *size_out = e.size;
            return 0;
        }
        count++;
    }
    return -1;
}

extern "C" void mfs_format(void) {
    if (!mfs_is_mounted) return;

    FsHeader h;
    h.magic = FS_MAGIC;
    h.file_count = 0;
    h.reserved0 = 0;
    h.reserved1 = 0;
    fs_write_header(&h);

    FsEntry zero;
    mem_zero(&zero, sizeof(zero));
    for (int i = 0; i < MFS_MAX_FILES; i++) {
        fs_write_entry(i, &zero);
    }
}
