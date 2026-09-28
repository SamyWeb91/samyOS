#include "paging.h"
#include "screen.h"

static unsigned int page_directory[1024] __attribute__((aligned(4096)));
static unsigned int page_table_0[1024]   __attribute__((aligned(4096)));

#define PT_POOL_SIZE 16
static unsigned int pt_pool[PT_POOL_SIZE][1024] __attribute__((aligned(4096)));
static int pt_pool_next = 0;

extern "C" void paging_enable_asm(unsigned int pd_phys);

extern "C" void paging_init() {
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0;
        page_table_0[i]   = 0;
    }

    for (int i = 0; i < 1024; i++) {
        page_table_0[i] = (i * 0x1000) | PTE_PRESENT | PTE_RW;
    }
    page_directory[0] = ((unsigned int)page_table_0) | PTE_PRESENT | PTE_RW;
}

extern "C" void paging_enable_now() {
    paging_enable_asm((unsigned int)page_directory);
}

extern "C" void paging_map(unsigned int virt, unsigned int phys, unsigned int flags) {
    unsigned int pd_index = virt >> 22;
    unsigned int pt_index = (virt >> 12) & 0x3FF;

    if (!(page_directory[pd_index] & PTE_PRESENT)) {
        if (pt_pool_next >= PT_POOL_SIZE) return;

        unsigned int *new_pt = pt_pool[pt_pool_next++];
        for (int i = 0; i < 1024; i++) new_pt[i] = 0;

        page_directory[pd_index] = ((unsigned int)new_pt) | PTE_PRESENT | PTE_RW;
    }

    unsigned int *pt = (unsigned int*)(page_directory[pd_index] & 0xFFFFF000);
    pt[pt_index] = (phys & 0xFFFFF000) | (flags & 0xFFF) | PTE_PRESENT;

    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

extern "C" void paging_map_range(unsigned int virt, unsigned int phys, unsigned int size_kb, unsigned int flags) {
    for (unsigned int i = 0; i < size_kb; i++) {
        paging_map(virt + i * 0x1000, phys + i * 0x1000, flags);
    }
}

extern "C" unsigned int paging_get_pd_addr()  { return (unsigned int)page_directory; }
extern "C" unsigned int paging_get_pt0_addr() { return (unsigned int)page_table_0; }
