// test.c - Programa de prueba para Ring 3

// Prototipo de la función de entrada
void _start(void);

// Syscall para escribir en pantalla (usando inline asm)
static inline int sys_write(int fd, const char *buf, unsigned int count) {
    int ret;
    __asm__ __volatile__ (
        "int $0x80"
        : "=a" (ret)
        : "a" (4), "b" (fd), "c" (buf), "d" (count)
        : "memory"
    );
    return ret;
}

// Syscall para salir
static inline void sys_exit(int code) {
    __asm__ __volatile__ (
        "int $0x80"
        : : "a" (1), "b" (code)
    );
}

// Función de entrada
void _start(void) {
    const char *msg = ">>> HOLA DESDE RING 3 EN C! <<<\n";
    
    // Calcular longitud de la cadena
    unsigned int len = 0;
    while (msg[len]) len++;

    // Imprimir
    sys_write(1, msg, len);
    
    // Bucle infinito (para que no retorne)
    while(1) {}
}
