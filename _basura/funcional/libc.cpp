#include "libc.h"

extern "C" void* memcpy(void* dst, const void* src, size_t n) {
    unsigned char *d = (unsigned char*)dst;
    const unsigned char *s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dst;
}

extern "C" void* memset(void* dst, int c, size_t n) {
    unsigned char *d = (unsigned char*)dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

extern "C" int memcmp(const void* a, const void* b, size_t n) {
    const unsigned char *x = (const unsigned char*)a;
    const unsigned char *y = (const unsigned char*)b;
    while (n--) {
        if (*x != *y) return *x - *y;
        x++; y++;
    }
    return 0;
}

extern "C" char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    if ((char)c == 0) return (char*)s;
    return 0;
}

extern "C" size_t strlen(const char* s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}
