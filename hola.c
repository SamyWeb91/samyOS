// hola.c - Programa de prueba para Ring 3
void _start() {
    // El bootloader guardo la direccion del framebuffer en la memoria.
    // En tu hardware real, vimos que es 0xE0000000.
    // Usamos un puntero a entero de 32 bits.
    unsigned int *fb = (unsigned int *)0xE0000000;

    // Pintamos toda la pantalla (1024x768) de color ROJO
    // En formato BGRA (Azul, Verde, Rojo, Alpha) el rojo puro es 0x00FF0000
    for (unsigned int i = 0; i < 1024 * 768; i++) {
        fb[i] = 0x00FF0000; 
    }

    // Bucle infinito para que el procesador no se salga del programa
    while(1);
}
