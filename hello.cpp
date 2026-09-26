// hello.cpp - Programa de usuario para MiniOS
// Compilar en la VPS con:
//   g++ -x c++ -m32 -ffreestanding -fno-pic -fno-stack-protector \
//       -fno-asynchronous-unwind-tables -fno-exceptions -fno-rtti \
//       -c hello.cpp -o hello.o
//   ld -m elf_i386 -Ttext 0x400000 -e _start --oformat binary \
//       -o hello.bin hello.o

#define SYS_EXIT     0
#define SYS_PRINT    1
#define SYS_PUTCHAR  2
#define SYS_GETCHAR  3
#define SYS_TICKS    4

static inline unsigned int syscall(unsigned int num,
                                    unsigned int a,
                                    unsigned int b,
                                    unsigned int c) {
    unsigned int ret;
    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(num), "b"(a), "c"(b), "d"(c)
                 : "memory");
    return ret;
}

static void print(const char *s) {
    syscall(SYS_PRINT, (unsigned int)s, 0, 0);
}

static void putchar(char c) {
    syscall(SYS_PUTCHAR, (unsigned char)c, 0, 0);
}

extern "C" void _start() {
    print("=== Programa de usuario ===\n");
    print("Hola desde el programa 0!\n");
    print("Ticks actuales: ");
    unsigned int t = syscall(SYS_TICKS, 0, 0, 0);
    putchar('0' + (t / 100) % 10);
    putchar('0' + (t / 10) % 10);
    putchar('0' + t % 10);
    putchar('\n');
    print("Pulsa una tecla para volver...\n");

    while (1) {
        char c = (char)syscall(SYS_GETCHAR, 0, 0, 0);
        if (c) {
            putchar('\n');
            print("Tecleaste: ");
            putchar(c);
            putchar('\n');
            break;
        }
    }

    print("Saliendo...\n");
    syscall(SYS_EXIT, 0, 0, 0);
    while (1) {}
}
