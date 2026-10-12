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
