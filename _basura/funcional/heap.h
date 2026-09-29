#pragma once

void  heap_init();
void* kmalloc(unsigned int size);
void  kfree(void* ptr);

// Estadísticas para la shell
struct HeapStats {
    unsigned int total_size;
    unsigned int used_size;
    unsigned int free_size;
    unsigned int num_blocks;
    unsigned int num_free;
};

HeapStats heap_get_stats();
