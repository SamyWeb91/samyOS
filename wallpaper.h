#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Carga un BMP 24-bit desde FAT32 y lo estampa en el backbuffer (o fb directo).
// Devuelve 1 si OK, 0 si falla.
int wallpaper_load(const char *path);

#ifdef __cplusplus
}
#endif
