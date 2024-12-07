#include "syscall.h"

void trap_handler(uint64_t scause, uint64_t sepc, struct pt_regs *regs){
    // 通过 `scause` 判断trap类型
    // 如果是interrupt 判断是否是timer interrupt
    // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
    // `clock_set_next_event()` 见 4.3.4 节
    // 其他interrupt / exception 可以直接忽略
    unsigned long interrupt = scause & 0x8000000000000000;
    if (interrupt){
        unsigned long timer_interrupt = (scause - 0x8000000000000000) & 0xff;
        if (timer_interrupt == 5){
            // printk("[S] Supervisor Mode Timer Interrupt\n");
            clock_set_next_event();
            do_timer();
        }
    }else{
        if (scause == 8){
            if (regs->x[17] == SYS_WRITE){
                regs->x[10] = sys_write(regs->x[10], (const char *)regs->x[11], regs->x[12]);
            }else if (regs->x[17] == SYS_GETPID){
                regs->x[10] = sys_getpid();
            }
            regs->sepc += 4;
        }
    }
}
