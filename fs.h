#pragma once

#include <stdint.h>

#define FS_MAX_FILES  16
#define FS_MAX_NAME   32
#define FS_MAX_SIZE   4096

#define FS_OK          0
#define FS_ERR_FULL   -1
#define FS_ERR_EXISTS -2
#define FS_ERR_NOENT  -3
#define FS_ERR_NOSPC  -4
#define FS_ERR_PARAM  -5

struct FSFile {
    char          name[FS_MAX_NAME];
    unsigned int  size;
    int           used;
    int           open;
};

#ifdef __cplusplus
extern "C" {
#endif

void fs_init(void);
int  fs_create(const char *name);
int  fs_open(const char *name);
int  fs_write(int fd, const void *buf, unsigned int len);
int  fs_read(int fd, void *buf, unsigned int len);
int  fs_seek(int fd, unsigned int offset);
int  fs_close(int fd);
int  fs_delete(const char *name);
void fs_list(void);
int  fs_size(int fd);

#ifdef __cplusplus
}
#endif
