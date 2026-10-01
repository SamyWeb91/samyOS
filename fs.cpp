#include "fs.h"
#include "screen.h"

static FSFile files[FS_MAX_FILES];
static unsigned char data[FS_MAX_FILES][FS_MAX_SIZE];
static unsigned int pos[FS_MAX_FILES];

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

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

extern "C" void fs_init(void) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        files[i].name[0] = 0;
        files[i].size    = 0;
        files[i].used    = 0;
        files[i].open    = 0;
        pos[i]           = 0;
    }
    kprintf("FS: inicializado (%d archivos max, %d bytes cada uno)\n",
            FS_MAX_FILES, FS_MAX_SIZE);
}

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

extern "C" int fs_create(const char *name) {
    if (!name || name[0] == 0) return FS_ERR_PARAM;
    if (find_by_name(name) >= 0) return FS_ERR_EXISTS;

    int idx = find_free();
    if (idx < 0) return FS_ERR_FULL;

    str_copy(files[idx].name, name, FS_MAX_NAME);
    files[idx].size = 0;
    files[idx].used = 1;
    files[idx].open = 0;
    pos[idx] = 0;
    return idx;
}

extern "C" int fs_open(const char *name) {
    if (!name) return FS_ERR_PARAM;

    int idx = find_by_name(name);
    if (idx < 0) {
        idx = fs_create(name);
        if (idx < 0) return idx;
    }

    files[idx].open++;
    pos[idx] = 0;
    return idx;
}

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
    return (int)len;
}

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

extern "C" int fs_seek(int fd, unsigned int offset) {
    if (fd < 0 || fd >= FS_MAX_FILES) return FS_ERR_PARAM;
    if (!files[fd].used) return FS_ERR_NOENT;
    if (offset > FS_MAX_SIZE) return FS_ERR_PARAM;

    pos[fd] = offset;
    return FS_OK;
}

extern "C" int fs_close(int fd) {
    if (fd < 0 || fd >= FS_MAX_FILES) return FS_ERR_PARAM;
    if (!files[fd].used) return FS_ERR_NOENT;

    if (files[fd].open > 0) files[fd].open--;
    return FS_OK;
}

extern "C" int fs_delete(const char *name) {
    if (!name) return FS_ERR_PARAM;

    int idx = find_by_name(name);
    if (idx < 0) return FS_ERR_NOENT;

    files[idx].used = 0;
    files[idx].size = 0;
    files[idx].name[0] = 0;
    files[idx].open = 0;
    pos[idx] = 0;
    return FS_OK;
}

extern "C" int fs_size(int fd) {
    if (fd < 0 || fd >= FS_MAX_FILES) return FS_ERR_PARAM;
    if (!files[fd].used) return FS_ERR_NOENT;
    return (int)files[fd].size;
}

extern "C" void fs_list(void) {
    int count = 0;
    unsigned int total = 0;

    kprintf("Archivos:\n");
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!files[i].used) continue;

        // Padding manual en lugar de %-16s (kprintf no lo soporta)
        kprintf("  %s", files[i].name);
        int len = str_len(files[i].name);
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
