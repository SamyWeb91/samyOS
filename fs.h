/*
 * Copyright (C) 2026 Samuel Tamayo Infante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>

#define FS_MAX_FILES  16
#define FS_MAX_NAME   32
#define FS_MAX_SIZE   4096

#define FS_OK          0
#define FS_ERR_FULL   -1
#define FS_ERR_EXISTS -2
#define FS_ERR_NOENT  -3
#define FS_ERR_NOSPC  -4
#define FS_ERR_PARAM  -5
#define FS_ERR_IO     -6

// --- Layout en disco ---
#define FS_DISK_LBA       2048
#define FS_SECTORS_PER_FILE  9   // 1 metadatos + 8 datos
#define FS_DATA_SECTORS      8   // 8 × 512 = 4096
#define FS_MAGIC          0x4D494E49u   // "MINI"

struct FSFile {
    char          name[FS_MAX_NAME];
    unsigned int  size;
    int           used;
    int           open;
};

#ifdef __cplusplus
extern "C" {
#endif

void fs_init(void);
int  fs_create(const char *name);
int  fs_open(const char *name);
int  fs_write(int fd, const void *buf, unsigned int len);
int  fs_read(int fd, void *buf, unsigned int len);
int  fs_seek(int fd, unsigned int offset);
int  fs_close(int fd);
int  fs_delete(const char *name);
void fs_list(void);
int  fs_size(int fd);

// --- Nuevas ---
void fs_flush(void);       // forzar escritura a disco
void fs_format(void);      // borrar todo el FS

#ifdef __cplusplus
}
#endif
