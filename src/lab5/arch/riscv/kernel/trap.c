#include "syscall.h"
#include "proc.h"
#include "string.h"
#include "defs.h"

extern struct task_struct *current;
extern char _sramdisk[];
void trap_handler(uint64_t scause, uint64_t sepc, struct pt_regs *regs)
{
    // 通过 `scause` 判断trap类型
    // 如果是interrupt 判断是否是timer interrupt
    // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
    // 其他interrupt / exception 可以直接忽略
    if (scause == 0x8000000000000005) {
        printk("[S] Supervisor Mode Timer Interrupt\n");
        clock_set_next_event();
        do_timer();
    }
    else if (scause == 8) {
        {
            if (regs->a7 == SYS_WRITE) {
                regs->a0 = sys_write(regs->a0, (const char *)regs->a1, regs->a2);
            }
            else if (regs->a7 == SYS_GETPID) {
                regs->a0 = sys_getpid();
            }
            else {
                printk("[S] Unhandled syscall: %lx", regs->a7);
                while (1);
            }
            regs->sepc += 4;
        }
    }
    else if (scause == 12 || scause == 13 || scause == 15) {
        switch (scause) {
            case 12: printk("Instruction Page Fault\n"); break;
            case 13: printk("Load Page Fault\n"); break;
            case 15: printk("Store Page Fault\n"); break;
            default: break;
        }
        printk("sepc = %lx, scause = %lx, stval = %lx\n", regs->sepc, regs->scause, regs->stval);
        do_page_fault(regs);
    }
    else {
        printk("[S] Unhandled trap, ");
        printk("scause: %lx, ", scause);
        printk("stval: %lx, ", regs->stval);
        printk("sepc: %lx\n", regs->sepc);
        while (1);
    }
}

void do_page_fault(struct pt_regs *regs)
{
    /*
     1. 通过 stval 获得访问出错的虚拟内存地址（Bad Address）
     2. 通过 find_vma() 查找 Bad Address 是否在某个 vma 中
     3. 分配一个页，将这个页映射到对应的用户地址空间
     4. 通过 (vma->vm_flags & VM_ANONYM) 获得当前的 VMA 是否是匿名空间
     5. 根据 VMA 匿名与否决定将新的页清零或是拷贝 uapp 中的内容
    */
    uint64_t bad_addr = regs->stval; // 获得访问出错的虚拟内存地址
    struct vm_area_struct *vma = find_vma(current, bad_addr); //查找bad_addr是否在某个vma中
    if (vma != NULL) {
        // 若找到bad_addr所在的vma
        uint64 new_page = (uint64)alloc_page();
        uint64 perm = vma->vm_flags | 0x11; // PTN_U | PTN_V
        create_mapping(current->pgd, PGROUNDDOWN(bad_addr), new_page - PA2VA_OFFSET, PGSIZE, perm);
        if (!(vma->vm_flags & VM_ANONYM)) {
            // 非匿名page，需要拷贝
            uint64 src = (uint64)_sramdisk + vma->vm_content_offset_in_file;
            uint64 offset = vma->vm_start % PGSIZE;
            if (PGROUNDUP(bad_addr) - vma->vm_start < PGSIZE) {
                // bad_addr 在 vma 的开头
                uint64 size = vma->vm_end - offset >= PGROUNDUP(bad_addr) ? PGSIZE-offset : vma->vm_content_size_in_file;
                memcpy((void *)new_page + offset, src, size);
            }
            else if (vma->vm_end - PGROUNDDOWN(bad_addr) < PGSIZE) {
                // bad_addr 在 vma 的末尾
                uint64 offset = vma->vm_end % PGSIZE;
                memcpy((void *)new_page, src, offset);
            }
            else {
                // 匿名拷贝，只需要简单映射
                memcpy((void *)new_page, src, PGSIZE);
            }
        }
    } else {
        printk("Not find %lx in vma. pid = %d\n", bad_addr, current->pid);
        while (1);
    }
    return;
}
