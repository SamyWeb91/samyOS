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

extern "C" void asm_sys_draw_rect(int x, int y, int w, int h, uint32_t color);
extern "C" void asm_sys_exit(int code);

extern "C" void ui_draw_rect(int x, int y, int w, int h, uint32_t color) {
    asm_sys_draw_rect(x, y, w, h, color);
}

extern "C" void ui_exit(int code) {
    asm_sys_exit(code);
}
