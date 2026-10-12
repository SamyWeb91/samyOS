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

#ifdef __cplusplus
}
#endif
