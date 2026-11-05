// ring3_test.cpp
#include "screen.h"
#include "paging.h"
#include "tss.h"
#include "fat32.h"
#include "ahci.h"
#include "process.h"
#include "thread.h"

#define USER_STACK_SIZE     0x4000u

static uint8_t file_buffer[65536];
static uint8_t ring3_kernel_stack[8192] __attribute__((aligned(16)));

// Variables globales para pasar los datos al hilo puente
static uint32_t global_entry = 0;
static uint32_t global_stack = 0;
static int global_pid = -1; // NUEVO: Para liberar la memoria al terminar

// Estructuras ELF
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
extern "C" int fat32_read_cluster_raw(uint32_t cluster, void *buf);
extern "C" uint32_t fat32_next_cluster(uint32_t current);

static uint32_t found_cluster = 0;
static uint32_t found_size = 0;
static const char* target_filename = 0;

static int str_match(const char *s1, const char *s2, int max_len) {
    for (int i = 0; i < max_len; i++) {
        char c1 = s1[i];
        char c2 = s2[i];
        if (c2 == '\0') {
            return (c1 == ' ' || c1 == '\0');
        }
        if (c1 != c2) return 0;
    }
    return 1;
}

static void find_file_cb(const char *name, uint32_t size, uint8_t attr, uint32_t cluster) {
    if (attr & 0x10) return;
    if (str_match(name, target_filename, 11)) {
        found_cluster = cluster;
        found_size = size;
    }
}

// =====================================================================
// HILO PUENTE: Se ejecuta en Ring 0 y hace el salto a Ring 3
// =====================================================================
static void jump_thread_func(void) {
    kprintf("jump_thread: Saltando a Ring 3 (Entry=0x%x)\n", global_entry);
    enter_user_mode(global_entry, global_stack);
    
    // Si el hilo regresa (porque el usuario hizo SYS_EXIT), liberamos la memoria
    process_free(global_pid);
    thread_exit();
}

// =====================================================================
// CARGADOR
// =====================================================================
extern "C" int ring3_execute(const char *filename) {
    kprintf("ring3: Iniciando programa '%s'...\n", filename);

    uint32_t base_addr = 0;
    uint32_t stack_top = 0;
    
    // 1. Pedir hueco (devuelve base 0x08000000, 0x09000000, etc.)
    int pid = process_alloc(filename, &base_addr, &stack_top);
    if (pid < 0) return -1;

    if (!fat32_is_mounted()) {
        uint32_t lba = fat32_scan();
        if (!lba || !fat32_mount(lba)) { process_free(pid); return -1; }
    }

    found_cluster = 0;
    found_size = 0;
    target_filename = filename;
    fat32_list("/", find_file_cb);

    if (found_cluster == 0) {
        kprintf("ring3: Archivo no encontrado\n");
        process_free(pid);
        return -1;
    }

    uint32_t bytes_to_read = found_size;
    if (bytes_to_read > sizeof(file_buffer)) {
        kprintf("ring3: Archivo demasiado grande\n");
        process_free(pid);
        return -1;
    }

    uint32_t cluster = found_cluster;
    uint32_t copied = 0;
    uint8_t *out = file_buffer;

    while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC && copied < bytes_to_read) {
        if (!fat32_read_cluster_raw(cluster, out + copied)) break;
        uint32_t chunk = 4096;
        if (chunk > bytes_to_read - copied) chunk = bytes_to_read - copied;
        copied += chunk;
        cluster = fat32_next_cluster(cluster);
    }

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)file_buffer;
    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
        kprintf("ring3: Formato ELF invalido\n");
        process_free(pid);
        return -1;
    }

    // 2. Mapear pila de usuario
    uint32_t user_stack_base = base_addr + 0x10000;
    for (uint32_t off = 0; off < USER_STACK_SIZE; off += 0x1000) {
        paging_map(user_stack_base + off, user_stack_base + off, PTE_PRESENT | PTE_RW | PTE_USER);
    }

    // 3. Mapear framebuffer
    uint32_t fb_addr = *(uint32_t*)0x6004;
    for (uint32_t off = 0; off < 1024 * 768 * 4; off += 0x1000) {
        paging_map(fb_addr + off, fb_addr + off, PTE_PRESENT | PTE_RW | PTE_USER | PTE_PAT);
    }

    // 4. Cargar segmentos PT_LOAD sumando la base_addr
    Elf32_Phdr *phdr = (Elf32_Phdr *)(file_buffer + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            // El ELF tiene direcciones virtuales relativas (ej. 0x0)
            uint32_t elf_vaddr = phdr[i].p_vaddr;
            
            // Mapeamos y copiamos en base_addr + elf_vaddr
            uint32_t vaddr_start = (base_addr + elf_vaddr) & ~0xFFFu;
            uint32_t vaddr_end   = (base_addr + elf_vaddr + phdr[i].p_memsz + 0xFFF) & ~0xFFFu;

            for (uint32_t addr = vaddr_start; addr < vaddr_end; addr += 0x1000) {
                paging_map(addr, addr, PTE_PRESENT | PTE_RW | PTE_USER);
            }

            uint8_t *dest = (uint8_t *)(base_addr + elf_vaddr);
            uint8_t *src  = file_buffer + phdr[i].p_offset;

            for (uint32_t j = 0; j < phdr[i].p_filesz; j++) dest[j] = src[j];
            for (uint32_t j = phdr[i].p_filesz; j < phdr[i].p_memsz; j++) dest[j] = 0;
        }
    }

    asm volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax", "memory");

    uint32_t esp0 = (uint32_t)(ring3_kernel_stack + sizeof(ring3_kernel_stack));
    esp0 &= ~0xFu;
    tss_set_kernel_stack(esp0);

    // 5. Configurar variables globales para el hilo
    global_entry = base_addr + ehdr->e_entry; // Sumamos la base al entry
    global_stack = user_stack_base + USER_STACK_SIZE - 16;
    global_pid = pid; // Guardamos el PID para liberar memoria después

    kprintf("ring3: Entry final = 0x%x | Stack = 0x%x\n", global_entry, global_stack);

    int tid = thread_create(filename, jump_thread_func);
    if (tid < 0) {
        kprintf("ring3: Error al crear hilo (tabla llena)\n");
        process_free(pid);
        return -1;
    }

    kprintf("ring3: Hilo creado. Multitarea activa.\n");
    return 0;
}
