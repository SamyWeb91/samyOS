// Copyright (C) 2026 Samuel Tamayo Infante
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// =====================================================================
// user_prog.cpp
//
// Este código corre en RING 3. Se copia a 0x400000 y se ejecuta
// en modo usuario.
//
// IMPORTANTE:
//   - NO puede llamar a funciones del kernel (kprintf, kmalloc, etc.)
//   - Solo puede usar int 0x80 (syscalls)
//   - No puede acceder a memoria del kernel (page fault)
//
// El compilador no lo sabe, pero si el código resultante tiene
// cualquier referencia a funciones del kernel, fallará al ejecutarse.
// =====================================================================

// Dirección del mensaje pre-copiado por ring3_loader_init()
#define USER_MSG_ADDR   0x400800

// Números de syscall (deben coincidir con syscall.h)
#define SYS_WRITE   1
#define SYS_EXIT    2

// =====================================================================
// Punto de entrada del programa de usuario
// =====================================================================
extern "C" void user_prog_start(void) {
    const char *msg = (const char *)USER_MSG_ADDR;
    unsigned int len = 18;   // "Hola desde RING 3!\n" = 18 chars

    // Syscall: write(fd=1, msg, len)
    //   eax = 1  (SYS_WRITE)
    //   ebx = msg
    //   ecx = len
    asm volatile(
        "int $0x80"
        :                                     // sin outputs
        : "a"(SYS_WRITE), "b"(msg), "c"(len)  // inputs
        : "memory"                            // clobber
    );

    // Syscall: exit(code=0)
    //   eax = 2  (SYS_EXIT)
    //   ebx = 0
    asm volatile(
        "int $0x80"
        :
        : "a"(SYS_EXIT), "b"(0)
        : "memory"
    );

    // Nunca llegamos aquí (exit no retorna)
    while (1) {}
}

// =====================================================================
// Marca de fin del programa
//
// El loader copia desde user_prog_start hasta user_prog_end.
// Al estar en el mismo archivo, user_prog_end está justo después
// del código, así que el tamaño es exacto.
// =====================================================================
extern "C" void user_prog_end(void) {}
