// cpu.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void cpu_init(void);
unsigned long long cpu_rdtsc(void);
void cpu_begin_work(void);
void cpu_end_work(int busy);
int  cpu_usage(void);

#ifdef __cplusplus
}
#endif
