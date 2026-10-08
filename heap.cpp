#include "heap.h"
#include "screen.h"

// El heap empieza en 0x400000 (4 MB), justo después de los primeros 4 MB
// que usa el kernel y las estructuras de la BIOS. Termina en 0x1400000 (20 MB).
// El paging mapea los primeros 128 MB, así que todo el heap cae en zona mapeada.
#define HEAP_START 0x400000
#define HEAP_SIZE  0x1000000   // 16 MB (hasta 0x1400000)

struct Block {
    unsigned int size;
    unsigned int free;
    Block* next;
};

static Block* head = 0;

// =====================================================================
// Inicializar el heap
// =====================================================================
void heap_init() {
    head = (Block*)HEAP_START;
    head->size = HEAP_SIZE - sizeof(Block);
    head->free = 1;
    head->next = 0;

    kprintf("heap: %u KB en 0x%x (hasta 0x%x)\n",
            HEAP_SIZE / 1024, HEAP_START, HEAP_START + HEAP_SIZE);
}

// =====================================================================
// kmalloc: busca el primer bloque libre que quepa
// =====================================================================
void* kmalloc(unsigned int size) {
    if (size == 0) return 0;

    // Alinear a 8 bytes
    size = (size + 7) & ~7;

    Block* b = head;
    while (b) {
        if (b->free && b->size >= size) {
            if (b->size >= size + sizeof(Block) + 8) {
                Block* new_block = (Block*)((unsigned int)b + sizeof(Block) + size);
                new_block->size = b->size - size - sizeof(Block);
                new_block->free = 1;
                new_block->next = b->next;

                b->size = size;
                b->next = new_block;
            }
            b->free = 0;
            return (void*)((unsigned int)b + sizeof(Block));
        }
        b = b->next;
    }
    return 0;
}

// =====================================================================
// kfree
// =====================================================================
void kfree(void* ptr) {
    if (!ptr) return;

    Block* b = (Block*)((unsigned int)ptr - sizeof(Block));
    b->free = 1;

    Block* cur = head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += sizeof(Block) + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

// =====================================================================
// Estadísticas
// =====================================================================
HeapStats heap_get_stats() {
    HeapStats s;
    s.total_size = HEAP_SIZE;
    s.used_size = 0;
    s.free_size = 0;
    s.num_blocks = 0;
    s.num_free = 0;

    Block* b = head;
    while (b) {
        s.num_blocks++;
        if (b->free) {
            s.free_size += b->size;
            s.num_free++;
        } else {
            s.used_size += b->size;
        }
        b = b->next;
    }

    return s;
}
