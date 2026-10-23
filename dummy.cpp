// dummy.cpp
// Reemplazo de fat32.o para probar si el problema es el tamaño
// o el contenido de fat32.cpp

extern "C" {

// Un array estático grande, similar a g_sector_buf + g_vol + g_cluster_buf
// de fat32.cpp, para forzar el mismo tamaño en .bss y .text
static unsigned char dummy_buffer[8192] __attribute__((used));

// Funciones vacías con los mismos nombres que exporta fat32.o,
// para que si algo las llama, no falle el enlace.
void fat32_mount(void) {}
void fat32_unmount(void) {}
int  fat32_is_mounted(void) { return 0; }
int  fat32_list(void) { return 0; }
int  fat32_read_file(void) { return 0; }
int  fat32_exists(void) { return 0; }
int  fat32_is_dir(void) { return 0; }
int  fat32_file_size(void) { return 0; }

} // extern "C"
