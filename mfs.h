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

// mfs.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MFS_RD_ID          0
#define MFS_MAX_FILES      16

/* Monta el mini-FS sobre el ramdisk. Devuelve 0 si ok, -1 si error. */
int mfs_mount(void);

/* ¿Está montado? */
int mfs_mounted(void);

/* Crea un archivo vacío. Devuelve 0 si ok, < 0 si error. */
int mfs_create(const char *name);

/* Borra un archivo. Devuelve 0 si ok, < 0 si error. */
int mfs_delete(const char *name);

/* Lee un archivo. Devuelve bytes leídos, o -1 si error. */
int mfs_read_file(const char *name, char *buf, unsigned int max);

/* Escribe un archivo (lo crea si no existe). Devuelve bytes escritos, o -1. */
int mfs_write_file(const char *name, const char *data, unsigned int len);

/* ¿Existe el archivo? 1 si sí, 0 si no. */
int mfs_exists(const char *name);

/* Número total de archivos. */
int mfs_count(void);

/* Obtiene el archivo i-ésimo (0..count-1).
   Devuelve 0 si ok, -1 si no existe.
   name_out: buffer de al menos 16 bytes.
   size_out: puede ser NULL. */
int mfs_get(int i, char *name_out, unsigned int *size_out);

/* Borra todos los archivos y formatea la tabla. */
void mfs_format(void);

#ifdef __cplusplus
}
#endif
