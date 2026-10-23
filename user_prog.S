# user_prog.S — programa de prueba ring3 (binario plano)
# Numeracion de syscalls: Linux i386 (coincide con syscall.h)
#   eax=4  sys_write(ebx=buf, ecx=len)
#   eax=1  sys_exit(ebx=codigo)

.section .text
.global _start
_start:
    # sys_write(msg, msg_len)
    mov $4, %eax                     # SYS_WRITE
    mov $msg, %ebx
    mov $msg_len, %ecx
    int $0x80

    # sys_exit(0)
    mov $1, %eax                     # SYS_EXIT
    xor %ebx, %ebx
    int $0x80

1:  jmp 1b

.section .rodata
msg:
    .ascii "[ring3] Hola desde espacio de usuario (flat binary)!\n"
    .ascii "[ring3] Cargador user_run_flat OK.\n"
msg_len = . - msg
