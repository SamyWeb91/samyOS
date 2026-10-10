#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa el controlador UHCI y enumera el primer HID.
// Retorna 1 si OK, 0 si falla o no encuentra UHCI.
int  usb_init(void);

// Retorna 1 si hay un dispositivo USB listo.
int  usb_is_ready(void);

// ¿Hay teclado USB detectado?
int  usb_have_kbd(void);

// ¿Hay ratón USB detectado?
int  usb_have_mouse(void);

// Polling: hay que llamarlo periódicamente desde el bucle del desktop.
void usb_poll(void);

// Estado del ratón USB.
int  usb_mouse_x(void);
int  usb_mouse_y(void);
int  usb_mouse_buttons(void);

// Teclado USB: devuelve 0 si no hay tecla, o el carácter.
char usb_keyboard_getchar(void);

#ifdef __cplusplus
}
#endif
