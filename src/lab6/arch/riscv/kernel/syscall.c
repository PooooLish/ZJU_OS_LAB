#include "defs.h"
#include "stdint.h"
#include "stddef.h"
#include "proc.h"
#include "syscall.h"

extern struct task_struct *current;
extern struct task_struct *task[NR_TASKS]; // 线程数组, 所有的线程都保存在此
extern void __ret_from_fork();
extern unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));
long sys_write(unsigned int fd, const char *buf, size_t count)
{
    long ret = 0;
    for (int i = 0; i < count; i++) {
        if (fd == 1) {
            printk("%c", buf[i]);
            ret++;
        }
    }
    return ret;
}

long sys_getpid() {
    return current->pid;
}

uint64 sys_clone(struct pt_regs *regs) {

    // 1. 创建一个新的task，复制 parent task 的整个页
    struct task_struct *child = (struct task_struct*)kalloc();
    memcpy((void*)child, (void*)current, PGSIZE);

    // 得到 task 数组中第一个没有被初始化的位置，将 child 置于此位置
    for(int i = 1; i < NR_TASKS; i++){
        if(task[i] == NULL) {
            child->pid = i;
            task[i] = child;
            break;
        }
    }

    // 为 child task 指定返回点 __ret_from_fork
    child->thread.ra = &__ret_from_fork;

    // 需要计算 struct pt_regs 在页中的偏移量，并进一步获得虚拟地址，进行深拷贝
    child->thread.sp = (struct pt_regs*)(child + (uint64)regs - PGROUNDDOWN((uint64)regs));
    memcpy((void*)child->thread.sp, (void*)regs, sizeof(struct pt_regs));


    // 2. 利用参数 regs 来计算出 child task 的对应的 pt_regs 的地址，并将其中的 a0, sp, sepc 设置成正确的值

    ((struct pt_regs*)(child->thread.sp))->a0 = 0;
    // 设置 child task 的 sp 寄存器
    ((struct pt_regs*)(child->thread.sp))->sp = (uint64)child->thread.sp;
    // 执行下一条 PC
    ((struct pt_regs*)(child->thread.sp))->sepc = regs->sepc + 4;
    // 3. 为 child task 申请 user stack, 并将 parent task 的 user stack 数据复制到其中。
    // 3.1. 同时将子 task 的 user stack 的地址保存在 thread_info->user_sp 中
    // 4. 为 child task 分配一个根页表, 并将 parent task 的根页表复制至 child task

    // 为 child task 创建页表，并深拷贝 parent task 的 swapper_pg_dir
    child->pgd = alloc_page();
    memset(child->pgd, 0x0, PGSIZE);
    memcpy(child->pgd, swapper_pg_dir, PGSIZE);

    // 为 child task 创建用户栈
    uint64_t child_stack = alloc_page();
    memcpy(child_stack, USER_END - PGSIZE, PGSIZE);
    // mode = 0b11111
    create_mapping(child->pgd, USER_END-PGSIZE, (uint64)child_stack - PA2VA_OFFSET, 1, 0b10001);


    // 5. 根据 parent task 的页表和 vma 来分配并拷贝 child task 在用户态会用到的内存
    for (int i = 0; i < current->vma_cnt; i++) {
        struct vm_area_struct *vma = &(current->vmas[i]);
        uint64_t addr = vma->vm_start;
        uint64_t* pgd = current->pgd;
        // 需要对 parent task 的页表进行逐级检查，以确定是否已经加载相应地址的内容到内存中
        uint64_t ent1 = pgd[(addr >> 30) & 0x1ff], ent2 = 0, ent3 = 0;

        while (addr < vma->vm_end) {
            // copy 的条件是：parent task 的 pte 为 valid
            if (ent1 & 0x1) {
                ent2 = ((uint64_t *)((uint64)(((ent1 >> 10) & 0xfffffffffff)<<12) + PA2VA_OFFSET))[(addr >> 21) & 0x1ff];
                if (ent2 & 0x1) {
                    ent3 = ((uint64_t *)((uint64)(((ent2 >> 10) & 0xfffffffffff)<<12) + PA2VA_OFFSET))[(addr >> 12) & 0x1ff];
                    if (ent3 & 0x1) {
                        // 拷贝 parent task 页面的内容
                        uint64_t page = alloc_page();
                        memcpy((void*)page, (void*)PGROUNDDOWN(addr), PGSIZE);
                        // 对页面进行映射，构造 child task 的页表
                        create_mapping((uint64)child->pgd, PGROUNDDOWN(addr), (uint64)page-PA2VA_OFFSET, 1, (vma->vm_flags & (~(uint64_t)VM_ANONYM)) | 0b10001);
                    }
                }
            }
            addr += PGSIZE;
        }
    }

    // 6. 返回子 task 的 pid
    printk("[S] New task: %d\n", child->pid);
    return (uint64)(child->pid);

}