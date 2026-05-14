/*
 * This program performs two invalid syscalls (generating user-to-kernel-to-user
 * transitions that leave entries in the LBR) and then calls nanosleep(), which
 * triggers the BPF program to take an LBR snapshot.
 */

    .file 1 "src/exercise.S"

    .equ INVALID_SYSCALL_NR, 0x1fff
    .equ __NR_nanosleep, 35
    .equ __NR_exit, 60

    .text
    .globl _start
_start:
    .loc 1 18 0
    mov $INVALID_SYSCALL_NR, %eax
    syscall

    .loc 1 22 0
    mov $INVALID_SYSCALL_NR, %eax
    syscall

    .loc 1 28 0
    lea timespec(%rip), %rdi
    xor %esi, %esi
    mov $__NR_nanosleep, %eax
    syscall

    .loc 1 33 0
    xor %edi, %edi
    mov $__NR_exit, %eax
    syscall

    .data
timespec:
    .quad 0         /* tv_sec */
    .quad 1000000   /* tv_nsec (1ms) */
