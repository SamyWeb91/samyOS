#include "paging.h"
#include "screen.h"

#define PAGE_PRESENT   0x1
#define PAGE_RW        0x2
#define PAGE_USER      0x4

static unsigned int page_directory[1024] __attribute__((aligned(4096)));
static unsigned int page_table_0[1024]   __attribute__((aligned(4096)));

extern "C" void paging_enable_asm(unsigned int pd_phys);

void paging_init() {
    // 1. Limpiar el Page Directory
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0;
    }

    // 2. Limpiar la Page Table
    for (int i = 0; i < 1024; i++) {
        page_table_0[i] = 0;
    }

    // 3. Identity mapping: los primeros 4 MB
    for (int i = 0; i < 1024; i++) {
        unsigned int phys = i * 0x1000;
        page_table_0[i] = phys | PAGE_PRESENT | PAGE_RW;
    }

    // 4. Meter la Page Table en el Page Directory
    unsigned int pt_phys = (unsigned int)page_table_0;
    page_directory[0] = pt_phys | PAGE_PRESENT | PAGE_RW;
}

void paging_enable_now() {
    unsigned int pd_phys = (unsigned int)page_directory;
    paging_enable_asm(pd_phys);
}

unsigned int paging_get_pd_addr() {
    return (unsigned int)page_directory;
}

unsigned int paging_get_pt0_addr() {
    return (unsigned int)page_table_0;
}
