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

#include <stdint.h>
#include <stddef.h>

// Números de syscall (deben coincidir con syscall.h del kernel)
#define SYS_WRITE       1
#define SYS_EXIT        2
#define SYS_READ_KEY    3
#define SYS_OPEN        4
#define SYS_READ        5
#define SYS_WRITE_FILE  6
#define SYS_CLOSE       7
#define SYS_CLEAR       8
#define SYS_YIELD       9
#define SYS_SIZE        10
#define SYS_SEEK        11
#define SYS_DELETE      12
#define SYS_TICKS       13
#define SYS_UPTIME      14
#define SYS_DATE        15
#define SYS_GETPID      16
#define SYS_SLEEP       17
#define SYS_PUTCHAR     18
#define SYS_WRITELN     19
#define SYS_STRLEN      20
#define SYS_MEMINFO     21
#define SYS_BEEP        22

// =====================================================================
// Wrappers inline (cada uno hace int 0x80)
// =====================================================================

static inline int sys_write(const char *buf, unsigned int len) {
    int ret;
    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(SYS_WRITE), "b"(buf), "c"(len)
                 : "memory");
    return ret;
}

static inline void sys_exit(int code) {
    asm volatile("int $0x80"
                 : : "a"(SYS_EXIT), "b"(code) : "memory");
    while (1) {}
}

static inline int sys_read_key(void) {
    int ret;
    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(SYS_READ_KEY)
                 : "memory");
    return ret;
}

static inline int sys_open(const char *name) {
    int ret;
    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(SYS_OPEN), "b"(name)
                 : "memory");
    return ret;
}

static inline int sys_read(int fd, void *buf, unsigned int len) {
    int ret;
    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(len)
                 : "memory");
    return ret;
}

static inline int sys_write_file(int fd, const void *buf, unsigned int len) {
    int ret;
    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(SYS_WRITE_FILE), "b"(fd), "c"(buf), "d"(len)
                 : "memory");
    return ret;
}

static inline int sys_close(int fd) {
    int ret;
    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(SYS_CLOSE), "b"(fd)
                 : "memory");
    return ret;
}

static inline void sys_clear(void) {
    asm volatile("int $0x80" : : "a"(SYS_CLEAR) : "memory");
}

static inline int sys_size(int fd) {
    int ret;
    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(SYS_SIZE), "b"(fd)
                 : "memory");
    return ret;
}

static inline void sys_putchar(char c) {
    asm volatile("int $0x80" : : "a"(SYS_PUTCHAR), "b"((int)c) : "memory");
}
