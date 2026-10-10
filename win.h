#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIN32_MAX        8
#define WIN32_TITLE_MAX  32
#define WIN32_COLS       48
#define WIN32_ROWS       18

#define SYS_WIN_CREATE   30
#define SYS_WIN_DESTROY  31
#define SYS_WIN_CLEAR    32
#define SYS_WIN_WRITE    33
#define SYS_WIN_RECT     34
#define SYS_WIN_TITLE    35

void win_init(void);
void win_redraw_all(void);

/* Syscalls */
int  win_sys_create(int x, int y, int w, int h, const char *title);
int  win_sys_destroy(int id);
int  win_sys_clear(int id);
int  win_sys_write(int id, const char *str, int len);
int  win_sys_rect(int id, int x, int y, int w, int h, uint32_t color);
int  win_sys_title(int id, const char *title);

/* Helpers para el desktop */
void win_handle_click(int mx, int my, int button);
int  win_get_focused(void);
void win_set_focused(int id);
int  win_get_rect(int id, int *x, int *y, int *w, int *h);

#ifdef __cplusplus
}
#endif
