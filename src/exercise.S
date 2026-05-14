/*
 * This program performs two invalid syscalls (generating user-to-kernel-to-user
 * transitions that leave entries in the LBR) and then calls nanosleep(), which
 * triggers the BPF program to take an LBR snapshot.
 * Between each syscall section there is a nop-style branch (jmp to next instruction)
 * so that the LBR also contains a userspace branch between the transitions.
 */

    .file 1 "src/exercise.S"

    .equ INVALID_SYSCALL_NR, 0x1fff
    .equ __NR_nanosleep, 35
    .equ __NR_exit, 60

    .text
    .globl _start
_start:
    .loc 1 19 0
    mov $INVALID_SYSCALL_NR, %eax
    .loc 1 21 0
    syscall

    .loc 1 24 0
    jmp 1f
1:
    .loc 1 27 0
    mov $INVALID_SYSCALL_NR, %eax
    .loc 1 29 0
    syscall

    .loc 1 32 0
    jmp 2f
2:
    .loc 1 35 0
    lea timespec(%rip), %rdi
    .loc 1 37 0
    xor %esi, %esi
    .loc 1 39 0
    mov $__NR_nanosleep, %eax
    .loc 1 41 0
    syscall

    .loc 1 44 0
    xor %edi, %edi
    .loc 1 46 0
    mov $__NR_exit, %eax
    .loc 1 48 0
    syscall

    .data
timespec:
    .quad 0         /* tv_sec */
    .quad 1000000   /* tv_nsec (1ms) */
