#include "syscall.h"

extern struct task_struct *current;
long sys_write(unsigned int fd, const char *buf, size_t count)
{
    long ret = 0;
    for (int i = 0; i < count; i++)
    {
        if (fd == 1){
            printk("%c", buf[i]);
            ret++;
        }
    }
    return ret;
}

long sys_getpid()
{
    return current->pid;
}