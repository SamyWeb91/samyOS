// ring3_test.cpp
#include "screen.h"
#include "paging.h"
#include "tss.h"
#include "fat32.h"
#include "ahci.h"

#define USER_CODE       0x08000000u
#define USER_STACK_BASE 0x08010000u
#define USER_STACK_SIZE 0x4000u
#define USER_STACK_TOP  (USER_STACK_BASE + USER_STACK_SIZE)

static uint8_t file_buffer[65536];
static uint8_t ring3_kernel_stack[8192] __attribute__((aligned(16)));

// Estructuras ELF32
typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

#define PT_LOAD 1

extern "C" void enter_user_mode(uint32_t entry, uint32_t user_stack);
extern "C" void paging_map(unsigned int virt, unsigned int phys, unsigned int flags);

// Funciones expuestas desde fat32_win.cpp
extern "C" int fat32_read_cluster_raw(uint32_t cluster, void *buf);
extern "C" uint32_t fat32_next_cluster(uint32_t current);

static uint32_t found_cluster = 0;
static uint32_t found_size = 0;

static void find_file_cb(const char *name, uint32_t size, uint8_t attr, uint32_t cluster) {
    if (name[0] == 'A' && name[1] == '.' && name[2] == 'B' && name[3] == 'I' && name[4] == 'N') {
        found_cluster = cluster;
        found_size = size;
    }
}

extern "C" void ring3_test_run(void) {
    kprintf("ring3: === CARGANDO A.BIN DESDE DISCO (ELF) ===\n");

    if (!fat32_is_mounted()) {
        kprintf("ring3: FAT32 no montada. Buscando...\n");
        uint32_t lba = fat32_scan();
        if (!lba || !fat32_mount(lba)) {
            kprintf("ring3: ERROR no se pudo montar FAT32.\n");
            return;
        }
        kprintf("ring3: FAT32 montada correctamente en LBA %u\n", lba);
    }

    found_cluster = 0;
    found_size = 0;
    fat32_list("/", find_file_cb);

    if (found_cluster == 0) {
        kprintf("ring3: ERROR A.BIN no encontrado.\n");
        return;
    }

    kprintf("ring3: A.BIN encontrado (Cl=%u, Tam=%u). Leyendo...\n", found_cluster, found_size);

    uint32_t bytes_to_read = found_size;
    if (bytes_to_read > sizeof(file_buffer)) bytes_to_read = sizeof(file_buffer);

    uint32_t cluster = found_cluster;
    uint32_t copied = 0;
    uint8_t *out = file_buffer;

    while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC && copied < bytes_to_read) {
        if (!fat32_read_cluster_raw(cluster, out + copied)) {
            kprintf("ring3: Error leyendo clúster %u\n", cluster);
            break;
        }

        uint32_t chunk = 4096;
        if (chunk > bytes_to_read - copied) chunk = bytes_to_read - copied;

        copied += chunk;
        cluster = fat32_next_cluster(cluster);
    }

    kprintf("ring3: Leidos %u bytes del disco.\n", copied);

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)file_buffer;
    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
        kprintf("ring3: ERROR El archivo no es un ELF valido.\n");
        return;
    }

    kprintf("ring3: ELF valido. Entry=0x%x, Headers=%d\n", ehdr->e_entry, ehdr->e_phnum);

    // Mapear pila
    for (uint32_t off = 0; off < USER_STACK_SIZE; off += 0x1000) {
        paging_map(USER_STACK_BASE + off, USER_STACK_BASE + off, PTE_PRESENT | PTE_RW | PTE_USER);
    }

    // Mapear framebuffer
    uint32_t fb_addr = *(uint32_t*)0x6004;
    for (uint32_t off = 0; off < 1024 * 768 * 4; off += 0x1000) {
        paging_map(fb_addr + off, fb_addr + off, PTE_PRESENT | PTE_RW | PTE_USER | PTE_PAT);
    }

    // Cargar segmentos (PT_LOAD)
    Elf32_Phdr *phdr = (Elf32_Phdr *)(file_buffer + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            kprintf("ring3: Segmento %d en 0x%x\n", i, phdr[i].p_vaddr);

            uint32_t vaddr_start = phdr[i].p_vaddr & 0xFFFFF000;
            uint32_t vaddr_end = (phdr[i].p_vaddr + phdr[i].p_memsz + 0xFFF) & 0xFFFFF000;
            for (uint32_t addr = vaddr_start; addr < vaddr_end; addr += 0x1000) {
                paging_map(addr, addr, PTE_PRESENT | PTE_RW | PTE_USER);
            }

            uint8_t *dest = (uint8_t *)phdr[i].p_vaddr;
            uint8_t *src = file_buffer + phdr[i].p_offset;
            for (uint32_t j = 0; j < phdr[i].p_filesz; j++) dest[j] = src[j];
            for (uint32_t j = phdr[i].p_filesz; j < phdr[i].p_memsz; j++) dest[j] = 0;
        }
    }

    asm volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax", "memory");

    // Configurar TSS.esp0
    uint32_t esp0 = (uint32_t)(ring3_kernel_stack + sizeof(ring3_kernel_stack));
    esp0 &= ~0xFu;
    tss_set_kernel_stack(esp0);

    // Llenar la pila con un patrón para detectar si el iret falla
    uint32_t *stack_ptr = (uint32_t *)USER_STACK_TOP;
    for(int i=1; i<10; i++) stack_ptr[-i] = 0xDEADBEEF;

    kprintf("ring3: Mapeo completado. Preparando salto...\n");
    kprintf("ring3: TSS.esp0 = 0x%x\n", esp0);
    kprintf("ring3: EIP = 0x%x, CS = 0x1b, SS = 0x23\n", ehdr->e_entry);

    // AQUI FORZAMOS EL CAMBIO DE ANILLO
    kprintf("ring3: EJECUTANDO IRETD AHORA!!!\n");
    enter_user_mode(ehdr->e_entry, USER_STACK_TOP);

    kprintf("ring3: Volvimos (esto no deberia pasar)\n");
}
