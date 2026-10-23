// fat.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Comandos del shell para FAT32 */
int fat_cmd_mount(int wid, const char *arg);   /* fatmount <lba> */
int fat_cmd_info(int wid);                     /* fatinfo */
int fat_cmd_ls(int wid, const char *arg);      /* fatls [path] */
int fat_cmd_cat(int wid, const char *arg);     /* fatcat <path> */
int fat_cmd_stat(int wid, const char *arg);    /* fatstat <path> */

/* Init: resetea estado */
void fat_init(void);

#ifdef __cplusplus
}
#endif
