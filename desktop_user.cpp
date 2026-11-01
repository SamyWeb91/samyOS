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

#include "libui.h"

#define COLOR_DESKTOP 0x0087CEEB
#define COLOR_TASKBAR 0x001A3A5C

extern "C" void _start() {
    // 1. Dibujar el fondo (Wallpaper)
    // Nota: Si tu framebuffer no es 1024x768, ajusta estos valores.
    ui_draw_rect(0, 0, 1024, 768, COLOR_DESKTOP);

    // 2. Dibujar la barra de tareas (abajo)
    // 768 - 28 = 740. La barra de tareas mide 28 pixeles de alto.
    ui_draw_rect(0, 740, 1024, 28, COLOR_TASKBAR);

    // 3. Dibujar un "botón de inicio" (para probar)
    // Un rectángulo rojo en la esquina inferior izquierda.
    ui_draw_rect(10, 744, 100, 20, 0x00FF0000);

    // 4. Bucle infinito
    // El programa se queda aquí, pero el framebuffer ya está pintado.
    // Esto demuestra que un escritorio hecho en C++ puede correr en Ring 3.
    while(1) {
        // Hacer nada
    }
}
