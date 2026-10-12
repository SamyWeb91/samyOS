// folder.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int  folder_open(int x, int y, int w, int h);
void folder_handle_key(int wid, char c);
void folder_handle_click(int wid, int mx, int my);
int  folder_is_folder(int wid);
void folder_close(int wid);

#ifdef __cplusplus
}
#endif
