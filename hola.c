/*
 * Copyright (C) 2026 Samuel Tamayo Infante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
