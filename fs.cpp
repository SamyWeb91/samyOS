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

#include "fs.h"
#include "screen.h"
#include "ahci.h"

// =====================================================================
// Estructuras en disco
// =====================================================================
struct FSSuperblock {
    uint32_t magic;
    uint32_t version;
    uint32_t num_files;
    uint32_t reserved;
    uint8_t  pad[496];
} __attribute__((packed));

struct FSFileMeta {
    char     name[FS_MAX_NAME];
    uint32_t size;
    uint32_t used;
    uint8_t  pad[472];
} __attribute__((packed));

// =====================================================================
// Cache en RAM
// =====================================================================
static FSFile   files[FS_MAX_FILES];
static unsigned char data[FS_MAX_FILES][FS_MAX_SIZE];
static unsigned int  pos[FS_MAX_FILES];
static uint8_t  dirty[FS_MAX_FILES];   // 1 = datos modificados, volcar en close

// =====================================================================
// Helpers de string
// =====================================================================
static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static void str_copy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

// =====================================================================
// Layout en disco
// =====================================================================
// LBA 2048           : superbloque
// LBA 2049 + i*9     : metadatos archivo i
// LBA 2050 + i*9 ..  : datos archivo i (8 sectores)
//
// IMPORTANTE: ahci_read_sector / ahci_write_sector retornan:
//   1 = exito
//   0 = error
// Por eso todas las comprobaciones usan "== 0" para detectar fallo.
// =====================================================================
static uint64_t meta_lba(int i)  { return FS_DISK_LBA + 1 + (uint64_t)i * FS_SECTORS_PER_FILE; }
static uint64_t data_lba(int i)  { return meta_lba(i) + 1; }

// =====================================================================
// Escritura/lectura de metadatos de UN archivo
// =====================================================================
static int write_meta(int i) {
    FSFileMeta m;
    for (int k = 0; k < FS_MAX_NAME; k++) m.name[k] = files[i].name[k];
    m.size = files[i].size;
    m.used = files[i].used ? 1 : 0;
    for (int k = 0; k < 472; k++) m.pad[k] = 0;

    if (ahci_write_sector(meta_lba(i), &m) == 0) {
        kprintf("FS: error escribiendo meta %d\n", i);
        return FS_ERR_IO;
    }
    return FS_OK;
}

static int read_meta(int i) {
    FSFileMeta m;
    if (ahci_read_sector(meta_lba(i), &m) == 0) {
        return FS_ERR_IO;
    }
    for (int k = 0; k < FS_MAX_NAME; k++) files[i].name[k] = m.name[k];
    files[i].name[FS_MAX_NAME - 1] = 0;
    files[i].size = m.size;
    files[i].used = m.used ? 1 : 0;
    files[i].open = 0;
    pos[i] = 0;
    return FS_OK;
}

// =====================================================================
// Escritura/lectura de datos de UN archivo (8 sectores)
// =====================================================================
static int write_data(int i) {
    uint64_t base = data_lba(i);
    for (int s = 0; s < FS_DATA_SECTORS; s++) {
        const void *src = &data[i][s * 512];
        if (ahci_write_sector(base + s, src) == 0) {
            kprintf("FS: error escribiendo datos %d sector %d\n", i, s);
            return FS_ERR_IO;
        }
    }
    return FS_OK;
}

static int read_data(int i) {
    uint64_t base = data_lba(i);
    for (int s = 0; s < FS_DATA_SECTORS; s++) {
        void *dst = &data[i][s * 512];
        if (ahci_read_sector(base + s, dst) == 0) {
            kprintf("FS: error leyendo datos %d sector %d\n", i, s);
            return FS_ERR_IO;
        }
    }
    return FS_OK;
}

// =====================================================================
// Superbloque
// =====================================================================
static int write_super(uint32_t num_files) {
    FSSuperblock sb;
    sb.magic     = FS_MAGIC;
    sb.version   = 1;
    sb.num_files = num_files;
    sb.reserved  = 0;
    for (int k = 0; k < 496; k++) sb.pad[k] = 0;

    if (ahci_write_sector(FS_DISK_LBA, &sb) == 0) {
        kprintf("FS: error escribiendo superbloque\n");
        return FS_ERR_IO;
    }
    return FS_OK;
}

static int read_super(FSSuperblock *sb) {
    if (ahci_read_sector(FS_DISK_LBA, sb) == 0) return FS_ERR_IO;
    return FS_OK;
}

// =====================================================================
// Formatear el FS (borra todo)
// =====================================================================
extern "C" void fs_format(void) {
    kprintf("FS: formateando disco (LBA %u)...\n", (unsigned)FS_DISK_LBA);

    for (int i = 0; i < FS_MAX_FILES; i++) {
        files[i].name[0] = 0;
        files[i].size    = 0;
        files[i].used    = 0;
        files[i].open    = 0;
        pos[i]           = 0;
        dirty[i]         = 0;
        for (int k = 0; k < FS_MAX_SIZE; k++) data[i][k] = 0;
    }

    write_super(0);

    // Escribir 16 metadatos vacíos
    for (int i = 0; i < FS_MAX_FILES; i++) {
        write_meta(i);
    }

    kprintf("FS: formateado\n");
}

// =====================================================================
// Init
// =====================================================================
extern "C" void fs_init(void) {
    if (!ahci_is_ready()) {
        kprintf("FS: AHCI no disponible, FS en RAM solamente\n");
        for (int i = 0; i < FS_MAX_FILES; i++) {
            files[i].name[0] = 0;
            files[i].size    = 0;
            files[i].used    = 0;
            files[i].open    = 0;
            pos[i]           = 0;
            dirty[i]         = 0;
        }
        return;
    }

    FSSuperblock sb;
    if (read_super(&sb) != FS_OK) {
        kprintf("FS: error leyendo superbloque, formateando\n");
        fs_format();
        return;
    }

    if (sb.magic != FS_MAGIC || sb.version != 1) {
        kprintf("FS: disco sin FS valido, formateando\n");
        fs_format();
        return;
    }

    // Cargar metadatos (los datos se cargan en fs_open)
    int count = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (read_meta(i) != FS_OK) {
            kprintf("FS: error leyendo meta %d\n", i);
        }
        dirty[i] = 0;
        if (files[i].used) count++;
    }

    kprintf("FS: cargado desde disco (%d archivos)\n", count);
}

// =====================================================================
// Búsqueda
// =====================================================================
static int find_free(void) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!files[i].used) return i;
    }
    return -1;
}

static int find_by_name(const char *name) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (files[i].used && str_eq(files[i].name, name)) return i;
    }
    return -1;
}

static int count_used(void) {
    int n = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) if (files[i].used) n++;
    return n;
}

// =====================================================================
// Create
// =====================================================================
extern "C" int fs_create(const char *name) {
    if (!name || name[0] == 0) return FS_ERR_PARAM;
    if (find_by_name(name) >= 0) return FS_ERR_EXISTS;

    int idx = find_free();
    if (idx < 0) return FS_ERR_FULL;

    str_copy(files[idx].name, name, FS_MAX_NAME);
    files[idx].size = 0;
    files[idx].used = 1;
    files[idx].open = 0;
    pos[idx]        = 0;
    dirty[idx]      = 0;

    // Limpiar datos en RAM
    for (int k = 0; k < FS_MAX_SIZE; k++) data[idx][k] = 0;

    write_meta(idx);
    write_super((uint32_t)count_used());

    return idx;
}

// =====================================================================
// Open (carga datos del disco a RAM)
// =====================================================================
extern "C" int fs_open(const char *name) {
    if (!name) return FS_ERR_PARAM;

    int idx = find_by_name(name);
    if (idx < 0) {
        idx = fs_create(name);
        if (idx < 0) return idx;
    }

    // Cargar datos solo si no están dirty (ya en RAM)
    if (!dirty[idx]) {
        read_data(idx);
    }

    files[idx].open++;
    pos[idx] = 0;
    return idx;
}

// =====================================================================
// Write (a RAM, marca dirty)
// =====================================================================
extern "C" int fs_write(int fd, const void *buf, unsigned int len) {
    if (fd < 0 || fd >= FS_MAX_FILES) return FS_ERR_PARAM;
    if (!files[fd].used) return FS_ERR_NOENT;
    if (!buf) return FS_ERR_PARAM;

    unsigned int p = pos[fd];
    if (p + len > FS_MAX_SIZE) return FS_ERR_NOSPC;

    const unsigned char *src = (const unsigned char*)buf;
    for (unsigned int i = 0; i < len; i++) {
        data[fd][p + i] = src[i];
    }

    pos[fd] = p + len;
    if (pos[fd] > files[fd].size) {
        files[fd].size = pos[fd];
    }
    dirty[fd] = 1;
    return (int)len;
}

// =====================================================================
// Read (desde RAM)
// =====================================================================
extern "C" int fs_read(int fd, void *buf, unsigned int len) {
    if (fd < 0 || fd >= FS_MAX_FILES) return FS_ERR_PARAM;
    if (!files[fd].used) return FS_ERR_NOENT;
    if (!buf) return FS_ERR_PARAM;

    unsigned int p = pos[fd];
    if (p >= files[fd].size) return 0;

    if (p + len > files[fd].size) {
        len = files[fd].size - p;
    }

    unsigned char *dst = (unsigned char*)buf;
    for (unsigned int i = 0; i < len; i++) {
        dst[i] = data[fd][p + i];
    }

    pos[fd] = p + len;
    return (int)len;
}

// =====================================================================
// Seek
// =====================================================================
extern "C" int fs_seek(int fd, unsigned int offset) {
    if (fd < 0 || fd >= FS_MAX_FILES) return FS_ERR_PARAM;
    if (!files[fd].used) return FS_ERR_NOENT;
    if (offset > FS_MAX_SIZE) return FS_ERR_PARAM;

    pos[fd] = offset;
    return FS_OK;
}

// =====================================================================
// Close (flush si dirty)
// =====================================================================
extern "C" int fs_close(int fd) {
    if (fd < 0 || fd >= FS_MAX_FILES) return FS_ERR_PARAM;
    if (!files[fd].used) return FS_ERR_NOENT;

    if (dirty[fd]) {
        write_data(fd);
        write_meta(fd);
        dirty[fd] = 0;
    }

    if (files[fd].open > 0) files[fd].open--;
    return FS_OK;
}

// =====================================================================
// Delete
// =====================================================================
extern "C" int fs_delete(const char *name) {
    if (!name) return FS_ERR_PARAM;

    int idx = find_by_name(name);
    if (idx < 0) return FS_ERR_NOENT;

    files[idx].used = 0;
    files[idx].size = 0;
    files[idx].name[0] = 0;
    files[idx].open = 0;
    pos[idx] = 0;
    dirty[idx] = 0;

    write_meta(idx);
    write_super((uint32_t)count_used());

    return FS_OK;
}

// =====================================================================
// Size
// =====================================================================
extern "C" int fs_size(int fd) {
    if (fd < 0 || fd >= FS_MAX_FILES) return FS_ERR_PARAM;
    if (!files[fd].used) return FS_ERR_NOENT;
    return (int)files[fd].size;
}

// =====================================================================
// Flush manual (por si acaso)
// =====================================================================
extern "C" void fs_flush(void) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (dirty[i]) {
            write_data(i);
            write_meta(i);
            dirty[i] = 0;
        }
    }
    write_super((uint32_t)count_used());
}

// =====================================================================
// List
// =====================================================================
extern "C" void fs_list(void) {
    int count = 0;
    unsigned int total = 0;

    kprintf("Archivos:\n");
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!files[i].used) continue;

        kprintf("  %s", files[i].name);
        int len = 0;
        while (files[i].name[len]) len++;
        for (int j = len; j < 16; j++) screen_putchar(' ');

        kprintf("  %u bytes", files[i].size);
        if (files[i].open > 0) kprintf("  [open]");
        kprintf("\n");

        count++;
        total += files[i].size;
    }
    if (count == 0) {
        kprintf("  (vacio)\n");
    } else {
        kprintf("Total: %d archivos, %u bytes\n", count, total);
    }
}
