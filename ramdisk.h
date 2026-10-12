#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define RAMDISK_MAX 4

/* Crea un ramdisk estatico (usando una zona de memoria ya existente).
   Devuelve el id (0..RAMDISK_MAX-1) o -1 si no hay hueco. */
int ramdisk_create_static(unsigned int base, unsigned int size);

/* Crea un ramdisk dinamico (con kmalloc).
   Devuelve el id o -1 si no hay hueco o no hay memoria. */
int ramdisk_create(unsigned int size);

/* Destruye un ramdisk (libera memoria si era dinamico). */
int ramdisk_destroy(int id);

/* Lee `len` bytes desde `offset` del ramdisk `id` hacia `dst`.
   Devuelve bytes leidos, o -1 si error. */
int ramdisk_read(int id, unsigned int offset, void *dst, unsigned int len);

/* Escribe `len` bytes desde `src` al ramdisk `id` en `offset`.
   Devuelve bytes escritos, o -1 si error. */
int ramdisk_write(int id, unsigned int offset, const void *src, unsigned int len);

/* Devuelve el tamaño del ramdisk, o 0 si no existe. */
unsigned int ramdisk_size(int id);

/* Devuelve 1 si el ramdisk existe, 0 si no. */
int ramdisk_exists(int id);

#ifdef __cplusplus
}
#endif
