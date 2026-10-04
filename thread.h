#pragma once

#include <stdint.h>

#define THREAD_MAX       8
#define THREAD_STACK     4096
#define THREAD_NAME_MAX  32

#define TH_READY    0
#define TH_RUNNING  1
#define TH_BLOCKED  2
#define TH_ZOMBIE   3

struct Thread {
    uint32_t esp;
    uint32_t stack_base;
    uint32_t stack_size;
    int      state;
    int      id;
    int      priority;
    char     name[THREAD_NAME_MAX];
};

#ifdef __cplusplus
extern "C" {
#endif

void  thread_init(void);
int   thread_create(const char *name, void (*entry)(void));
void  thread_yield(void);
void  thread_exit(void);
int   thread_kill(int id);
int   thread_current_id(void);
void  thread_list(void);
void  schedule(void);

#ifdef __cplusplus
}
#endif
