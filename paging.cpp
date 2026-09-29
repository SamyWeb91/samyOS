#include "paging.h"
#include "screen.h"

static unsigned int page_directory[1024] __attribute__((aligned(4096)));

// 4 Page Tables para los primeros 16 MB
static unsigned int page_table_0[1024] __attribute__((aligned(4096)));
static unsigned int page_table_1[1024] __attribute__((aligned(4096)));
static unsigned int page_table_2[1024] __attribute__((aligned(4096)));
static unsigned int page_table_3[1024] __attribute__((aligned(4096)));

// Pool de Page Tables para mapear zonas fuera de los 16 MB (framebuffer, etc.)
#define PT_POOL_SIZE 16
static unsigned int pt_pool[PT_POOL_SIZE][1024] __attribute__((aligned(4096)));
static int pt_pool_next = 0;

extern "C" void paging_enable_asm(unsigned int pd_phys);

extern "C" void paging_init() {
    // Limpiar el Page Directory
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0;
    }

    // Limpiar las 4 Page Tables
    for (int i = 0; i < 1024; i++) {
        page_table_0[i] = 0;
        page_table_1[i] = 0;
        page_table_2[i] = 0;
        page_table_3[i] = 0;
    }

    // PT0: 0x000000 - 0x3FFFFF (4 MB)
    for (int i = 0; i < 1024; i++) {
        page_table_0[i] = (0x000000 + i * 0x1000) | PTE_PRESENT | PTE_RW;
    }
    page_directory[0] = ((unsigned int)page_table_0) | PTE_PRESENT | PTE_RW;

    // PT1: 0x400000 - 0x7FFFFF (4 MB)
    for (int i = 0; i < 1024; i++) {
        page_table_1[i] = (0x400000 + i * 0x1000) | PTE_PRESENT | PTE_RW;
    }
    page_directory[1] = ((unsigned int)page_table_1) | PTE_PRESENT | PTE_RW;

    // PT2: 0x800000 - 0xBFFFFF (4 MB)
    for (int i = 0; i < 1024; i++) {
        page_table_2[i] = (0x800000 + i * 0x1000) | PTE_PRESENT | PTE_RW;
    }
    page_directory[2] = ((unsigned int)page_table_2) | PTE_PRESENT | PTE_RW;

    // PT3: 0xC00000 - 0xFFFFFF (4 MB)
    for (int i = 0; i < 1024; i++) {
        page_table_3[i] = (0xC00000 + i * 0x1000) | PTE_PRESENT | PTE_RW;
    }
    page_directory[3] = ((unsigned int)page_table_3) | PTE_PRESENT | PTE_RW;

    // 16 MB mapeados en total
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
