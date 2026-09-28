#include "paging.h"
#include "screen.h"

// Flags de las entradas de pagina
#define PAGE_PRESENT   0x1
#define PAGE_RW        0x2
#define PAGE_USER      0x4

// El Page Directory debe estar alineado a 4 KB
static unsigned int page_directory[1024] __attribute__((aligned(4096)));

// Una Page Table para identity mapping de los primeros 4 MB
// 4 MB = 1024 paginas de 4 KB = 1 Page Table completa
static unsigned int page_table_0[1024] __attribute__((aligned(4096)));

// Funcion en ensamblador para cargar CR3 y activar paginacion
extern "C" void paging_enable(unsigned int pd_phys);

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
    // Direccion virtual X -> Direccion fisica X
    // flags: present + read-write
    for (int i = 0; i < 1024; i++) {
        unsigned int phys = i * 0x1000;    // i * 4 KB
        page_table_0[i] = phys | PAGE_PRESENT | PAGE_RW;
    }

    // 4. Meter la Page Table en el Page Directory
    // La Page Table cubre los primeros 4 MB (indice 0)
    unsigned int pt_phys = (unsigned int)page_table_0;
    page_directory[0] = pt_phys | PAGE_PRESENT | PAGE_RW;

    // 5. Activar paginacion
    unsigned int pd_phys = (unsigned int)page_directory;
    paging_enable(pd_phys);
}
