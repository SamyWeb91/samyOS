#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void paging_init();
void paging_enable_now();

void paging_map(unsigned int virt, unsigned int phys, unsigned int flags);
void paging_map_range(unsigned int virt, unsigned int phys, unsigned int size_kb, unsigned int flags);

unsigned int paging_get_pd_addr();
unsigned int paging_get_pt0_addr();

#ifdef __cplusplus
}
#endif

#define PTE_PRESENT  0x1
#define PTE_RW       0x2
#define PTE_USER     0x4
