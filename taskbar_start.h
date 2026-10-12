//code
#pragma once
#define START_ACT_NONE       0
#define START_ACT_TERMINAL   1
#define START_ACT_ABOUT      2
#define START_ACT_REBOOT     3
#define START_ACT_CLOSE_ALL  4
#ifdef __cplusplus
extern "C" {
#endif
/* Inicializa el modulo. Registra el callback para las acciones del menu. */
void taskbar_start_init(void (*action_cb)(int));

/* Devuelve 1 si el click fue consumido por el boton o el menu. */
int  taskbar_start_click(int mx, int my);

/* Devuelve 1 si el hover cambio algo visual. */
int  taskbar_start_hover(int mx, int my);

/* Dibuja el boton + menu si esta abierto. */
void taskbar_start_draw(void);

/* Devuelve 1 si el menu esta abierto. */
int  taskbar_start_is_open(void);

/* Fuerza el cierre del menu (por ejemplo, al perder el foco). */
void taskbar_start_close(void);

#ifdef __cplusplus
}
#endif
