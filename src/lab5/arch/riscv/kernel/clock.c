#include "sbi.h"
#include "clock.h"

// QEMU中时钟的频率是10MHz, 也就是1秒钟相当于10000000个时钟周期。
unsigned long TIMECLOCK = 10000000;

unsigned long get_cycles()
{
    unsigned long cycles;
    // 使用内联汇编读取mtime寄存器的值
    __asm__ volatile("rdtime %[cycles]\n"
                     : [cycles] "=r"(cycles)::);
    return cycles;
}

void clock_set_next_event()
{
    // 下一次时钟中断的时间点
    unsigned long next = get_cycles() + TIMECLOCK;

    // 使用sbi_ecall设置下一次时钟中断事件
    sbi_ecall(0, 0, next, 0, 0, 0, 0, 0);
}
