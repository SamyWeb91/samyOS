#pragma once
#include <stddef.h>

extern "C" void* memcpy(void* dst, const void* src, size_t n);
extern "C" void* memset(void* dst, int c, size_t n);
extern "C" int   memcmp(const void* a, const void* b, size_t n);
extern "C" char* strchr(const char* s, int c);
extern "C" size_t strlen(const char* s);
