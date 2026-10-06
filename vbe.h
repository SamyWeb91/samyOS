#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa VBE leyendo la info del bootloader (0x6000).
void vbe_init(void);

// Retorna 1 si VBE está disponible y el modo se cambió.
int vbe_available(void);

// Info del modo actual.
uint32_t vbe_fb_addr(void);
uint16_t vbe_pitch(void);
uint16_t vbe_width(void);
uint16_t vbe_height(void);
uint8_t  vbe_bpp(void);

#ifdef __cplusplus
}
#endif
