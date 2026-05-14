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
    .loc 1 20 0
    mov $INVALID_SYSCALL_NR, %eax
    syscall

    .loc 1 23 0
    jmp 1f
1:

    .loc 1 28 0
    mov $INVALID_SYSCALL_NR, %eax
    syscall

    .loc 1 31 0
    jmp 2f
2:

    .loc 1 38 0
    lea timespec(%rip), %rdi
    xor %esi, %esi
    mov $__NR_nanosleep, %eax
    syscall

    .loc 1 43 0
    xor %edi, %edi
    mov $__NR_exit, %eax
    syscall

    .data
timespec:
    .quad 0         /* tv_sec */
    .quad 1000000   /* tv_nsec (1ms) */
