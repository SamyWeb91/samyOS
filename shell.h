#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define SHELL_MAX_TERMS 4

/* Inicializa el subsistema de shell. Devuelve 0 si ok. */
int shell_init(void);

/* Crea una terminal nueva. Devuelve el wid, o -1 si no hay espacio. */
int shell_new_terminal(int x, int y, int w, int h, const char *title);

/* Procesa una tecla para la terminal wid. */
void shell_handle_key(int wid, char c);

/* Cierra la terminal wid. */
void shell_close_terminal(int wid);

/* Devuelve 1 si el wid es una terminal gestionada por el shell. */
int shell_is_terminal(int wid);

#ifdef __cplusplus
}
#endif
