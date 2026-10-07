#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa el ratón PS/2.
// Retorna 1 si OK, 0 si falla.
int mouse_init(void);

// Retorna 1 si el ratón está listo.
int mouse_is_ready(void);

// Posición actual del cursor.
int mouse_get_x(void);
int mouse_get_y(void);

// Estado de los botones.
int mouse_left_pressed(void);
int mouse_right_pressed(void);

// Handler de IRQ12 (llamado desde isr_handler).
void mouse_handler(void);

#ifdef __cplusplus
}
#endif
