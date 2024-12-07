#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "proc.h"
#include "types.h"
#include "printk.h"

#define SYS_WRITE   64
#define SYS_GETPID  172

struct pt_regs {
    uint64 x[32];
    uint64 sepc;
};

long sys_write(unsigned int fd, const char* buf, size_t count);
long sys_getpid();

#endif