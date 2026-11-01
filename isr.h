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

#pragma once

struct Regs {
    // pusha
    unsigned int edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    // stub
    unsigned int int_no, err_code;
    // CPU
    unsigned int eip, cs, eflags, useresp, ss;
};

extern "C" void isr_handler(Regs *r);
