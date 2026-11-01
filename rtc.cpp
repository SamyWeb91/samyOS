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

#include "rtc.h"
#include "io.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static unsigned char cmos_read(unsigned char reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static int cmos_update_in_progress() {
    return cmos_read(0x0A) & 0x80;
}

static unsigned char bcd_to_bin(unsigned char v) {
    return (v & 0x0F) + ((v >> 4) * 10);
}

void rtc_read(RTCTime *t) {
    // Esperar a que no haya update en progreso
    while (cmos_update_in_progress());

    unsigned char sec  = cmos_read(0x00);
    unsigned char min  = cmos_read(0x02);
    unsigned char hour = cmos_read(0x04);
    unsigned char day  = cmos_read(0x07);
    unsigned char mon  = cmos_read(0x08);
    unsigned char year = cmos_read(0x09);

    // Verificar que no cambió (por si el update empezó a mitad)
    while (cmos_update_in_progress());

    t->second = bcd_to_bin(sec);
    t->minute = bcd_to_bin(min);
    t->hour   = bcd_to_bin(hour);
    t->day    = bcd_to_bin(day);
    t->month  = bcd_to_bin(mon);
    t->year   = 2000 + bcd_to_bin(year);   // Asumimos siglo 21
}
