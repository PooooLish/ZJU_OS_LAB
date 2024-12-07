#include "printk.h"
#include "defs.h"
#include "proc.h"

extern struct task_struct* task[NR_TASKS];

void test() {
    printk("idle process is running!\n");
    while(1) {}
}