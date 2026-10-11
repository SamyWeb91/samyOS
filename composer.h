// composer.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa el compositor. Llamar despues de gfx_backbuffer_init()
   y mouse_init(), antes de crear la terminal. */
void composer_init(void);

/* Registra la ventana de la terminal (unica ventana conocida por
   el compositor de antemano). */
void composer_set_terminal(int wid);

/* Procesa el raton: posicion, botones, drag de ventanas.
   Devuelve 1 si hubo cambios visuales. */
int composer_handle_mouse(void);

/* Procesa un caracter de teclado. */
void composer_handle_key(char c);

/* Refresco periodico (reloj, CPU). Llamar cada iteracion del bucle.
   Devuelve 1 si marco algo sucio. */
int composer_periodic(unsigned int now_ticks);

/* ¿Hay algo sucio pendiente de redibujar? */
int composer_has_dirty(void);

/* Aplica el dirty: redibuja y flipea. */
void composer_apply(void);

/* Marca una ventana entera como sucia (para redibujarla). */
void composer_dirty_window(int wid);

/* Fuerza un redibujado completo. */
void composer_force_redraw(void);

#ifdef __cplusplus
}
#endif
