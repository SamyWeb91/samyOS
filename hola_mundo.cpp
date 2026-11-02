// hola_mundo.cpp

#define SYS_EXIT          0
#define SYS_DRAW_TEXT     13
#define SYS_CREATE_WINDOW 14

// Llamadas al sistema
inline void sys_draw_text(int x, int y, unsigned int color, const char* str) {
    asm volatile("int $0x80" : : "a"(SYS_DRAW_TEXT), "b"(x), "c"(y), "d"(color), "S"(str) : "memory");
}

inline int sys_create_window(int x, int y, int w, int h, const char* title) {
    int wid;
    asm volatile("int $0x80" : "=a"(wid) : "a"(SYS_CREATE_WINDOW), "b"(x), "c"(y), "d"(w), "S"(h), "D"(title) : "memory");
    return wid;
}

inline void sys_exit(int code) {
    asm volatile("int $0x80" : : "a"(SYS_EXIT), "b"(code) : "memory");
}

extern "C" void _start() {
    // 1. Pedir al kernel que cree una ventana real (x, y, ancho, alto, titulo)
    int wid = sys_create_window(200, 150, 400, 300, "Mi Primera App");
    
    if (wid >= 0) {
        // 2. Dibujar texto dentro de esa ventana
        // Nota: Las coordenadas aquí son absolutas de la pantalla.
        // Si dibujas en x=250, y=250, aparecerá dentro de la ventana.
        sys_draw_text(250, 250, 0x00000000, "Hola Mundo desde Ring 3!");
        sys_draw_text(250, 270, 0x00000000, "Ventana creada por una Syscall.");
    }

    // 3. Salir (esto cierra el programa y vuelve a la shell)
    sys_exit(0);

    while(1) {}
}
