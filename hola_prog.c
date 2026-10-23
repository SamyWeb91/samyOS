// hola_prog.c
#define SYS_WRITE 4
#define SYS_EXIT  1

void _start(void) {
    // Un mensaje simple
    const char msg[] = "Hola desde Ring 3!\n";
    unsigned int len = 18; // Longitud exacta de "Hola desde Ring 3!\n"

    // Syscall write
    __asm__ __volatile__ (
        "int $0x80"
        : 
        : "a"(SYS_WRITE), "b"(msg), "c"(len)
        : "memory"
    );

    // Syscall exit
    __asm__ __volatile__ (
        "int $0x80"
        : 
        : "a"(SYS_EXIT), "b"(0)
        : "memory"
    );

    while (1) {}
}
