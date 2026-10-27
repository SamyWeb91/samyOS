// disk.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Comandos. Devuelven 1 si consumieron el comando, 0 si no. */
int  disk_cmd_hex(int wid, const char *arg);
int  disk_cmd_partitions(int wid);
int  disk_cmd_disk(int wid);

/* Registra los comandos en el shell. */
void disk_init(void);

#ifdef __cplusplus
}
#endif
