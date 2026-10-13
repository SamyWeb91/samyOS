// shell.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define SHELL_MAX_TERMS 4

int  shell_init(void);
int  shell_new_terminal(int x, int y, int w, int h, const char *title);
void shell_handle_key(int wid, char c);
void shell_close_terminal(int wid);
int  shell_is_terminal(int wid);

/* Helpers de impresion para otros modulos (disk.cpp, folder.cpp, etc.) */
void shell_term_print(int wid, const char *s);
void shell_term_println(int wid, const char *s);
void shell_print_uint(int wid, unsigned int v);

#ifdef __cplusplus
}
#endif
