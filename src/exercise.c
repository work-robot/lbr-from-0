#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

/*
This program performs two invalid syscalls (generating user-to-kernel-to-user transitions that leave entries in the LBR)
and then calls nanosleep(), which triggers the BPF program to take an LBR snapshot.
*/

int main(void)
{
    struct timespec req = { .tv_sec = 0, .tv_nsec = 1000000 };

    syscall(0x1fff);
    syscall(0x1fff);
    nanosleep(&req, NULL);

    return 0;
}
