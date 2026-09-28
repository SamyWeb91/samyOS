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
