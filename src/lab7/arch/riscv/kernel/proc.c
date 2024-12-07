// arch/riscv/kernel/proc.c
#include "proc.h"
#include "mm.h"
#include "defs.h"
#include "rand.h"
#include "printk.h"
#include "elf.h"
#include "string.h"
#include "vm.h"
#include "virtio.h"
#include "mbr.h"

// arch/riscv/kernel/proc.c

extern void __dummy();

extern unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

struct task_struct *idle;           // idle process
struct task_struct *current_task;   // 指向当前运行线程的 `task_struct`
struct task_struct *task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

extern char _sramdisk[];
extern char _eramdisk[];
static char *uapp_start = _sramdisk;
static char *uapp_end = _eramdisk;

struct task_struct *get_current_task() {
    return current_task;
}

struct task_struct *get_task(int pid) {
    return task[pid];
}

struct task_struct *set_task(int pid, struct task_struct *new_task) {
    struct task_struct *old_task = task[pid];
    task[pid] = new_task;
    return old_task;
}

void do_mmap(struct task_struct *task, uint64_t addr, uint64_t length, uint64_t flags,
             uint64_t vm_content_offset_in_file, uint64_t vm_content_size_in_file) {
    // 1. 为 task 分配一个新的 VMA
    struct vm_area_struct *vma = &task->vmas[task->vma_cnt++];
    // 2. 设置 VMA 的 vm_start, vm_end, vm_flags, vm_content_offset_in_file, vm_content_size_in_file
    vma->vm_start = addr;
    vma->vm_end = addr + length;
    vma->vm_flags = flags;
    vma->vm_content_offset_in_file = vm_content_offset_in_file;
    vma->vm_content_size_in_file = vm_content_size_in_file;
}

struct vm_area_struct *find_vma(struct task_struct *task, uint64_t addr) {
    for (int i = 0; i < task->vma_cnt; i++) {
        if (task->vmas[i].vm_start <= addr && addr < task->vmas[i].vm_end) {
            return &task->vmas[i];
        }
    }
    return NULL;
}

void task_init() {

    idle = (struct task_struct *) kalloc();
    idle->state = TASK_RUNNING;
    idle->counter = 0;
    idle->priority = 0;
    idle->pid = 0;
    current_task = idle;
    task[0] = idle;

    for (int i = 1; i < 2; ++i) {
        task[i] = (struct task_struct *) kalloc();
        task[i]->state = TASK_RUNNING;
        task[i]->counter = rand() % 10 + 1;
        task[i]->priority = 1;
        task[i]->pid = i;
        task[i]->thread.ra = (uint64) __dummy;
        task[i]->thread.sp = (uint64) task[i] + PGSIZE;

        task[i]->thread.sepc = ((Elf64_Ehdr *) uapp_start)->e_entry;
        task[i]->thread.sstatus = SPP(SPP_USER) | SPIE(1) | SUM(1);
        task[i]->thread.sscratch = USER_END;

        task[i]->pgd = (pagetable_t) alloc_page();
        memcpy((char *) task[i]->pgd, &swapper_pg_dir, PGSIZE);

        map_uapp_elf(task[i]);
        do_mmap(task[i], USER_END - PGSIZE, PGSIZE, VM_R_MASK | VM_W_MASK | VM_ANONYM, 0, 0);

        task[i]->files = file_init();
    }

    for (int i = 2; i < NR_TASKS; ++i) {
        task[i] = NULL;
    }

    printk("[S] proc_init done!\n");

    virtio_dev_init();
    printk("[S] virtio_dev_init done!\n");
    mbr_init();
    printk("[S] mbr_init done!\n");
}

void map_uapp_bin(struct task_struct *t) {
}

void map_uapp_elf(struct task_struct *t) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *) uapp_start; // 获取ELF文件头

    // 获取ELF进程文件头序列首元素（e_phoff：ELF程序文件头数组相对ELF文件头的偏移地址）
    uint64 phdr_start = (uint64) ehdr + ehdr->e_phoff;

    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr_curr = (Elf64_Phdr *) (phdr_start + i * sizeof(Elf64_Phdr));
        if (phdr_curr->p_type == PT_LOAD) {
            do_mmap(t, phdr_curr->p_vaddr, phdr_curr->p_memsz, phdr_curr->p_flags << 1,
                    phdr_curr->p_offset, phdr_curr->p_filesz);
        }
    }
}

void dummy() {
//    schedule_test();
    uint64 MOD = 1000000007;
    uint64 auto_inc_local_var = 0;
    int last_counter = -1;
    while (1) {
        if ((last_counter == -1 || current_task->counter != last_counter) && current_task->counter > 0) {
            if (current_task->counter == 1) {
                --(current_task->counter); // forced the counter to be zero if this thread is going to be scheduled
            }                         // in case that the new counter is also 1, leading the information not printed.
            last_counter = current_task->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. auto_inc_local_var = %d, counter = %d\n", current_task->pid,
                   auto_inc_local_var,
                   current_task->counter);
        }
    }
}

extern void __switch_to(struct task_struct *prev, struct task_struct *next);

void switch_to(struct task_struct *next) {
    if (current_task == next)
        return;

//    printk("switch from [PID = %d] to [PID = %d]\n", (int) current->pid, (int) next->pid);

    struct task_struct *prev = current_task;
    current_task = next;
    __switch_to(prev, next);
}

void do_timer(void) {
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行调度

    // printk("current pid: %d, counter: %d\n", (int) current->pid, (int) current->counter);
    if (current_task == idle)
        schedule();
    else {
        if (current_task->counter > 0)
            current_task->counter--;
//        printk("[PID = %d] counter = %d\n",current->pid,current->counter);
        if (current_task->counter <= 0)
            schedule();
    }
}

#ifdef SJF

void schedule() {

    struct task_struct *next = idle;
    int counter = 0x7fffffff;
    int zero = current_task->counter == 0;
    for (int i = 1; i < NR_TASKS && task[i]; ++i) {
        if (task[i]->counter > 0 && task[i]->counter < counter) {
            next = task[i];
            counter = task[i]->counter;
        }
        if (zero && task[i]->counter != 0)
            zero = 0;
    }

    if (zero) {
        for (int i = 1; i < NR_TASKS && task[i]; ++i) {
            task[i]->counter = rand() % 10 + 1;
        }
        schedule();
        return;
    }

    printk("SJF: switch to [PID=%d], counter=%d\n", next->pid, next->counter);
    switch_to(next);
}

#endif

#ifdef PRIORITY

void schedule() {
    // printk("PRIORITY schedule\n");

    uint64 i, next, c;
    struct task_struct **p;

    while (1) {
        c = 0;
        next = 0;
        i = NR_TASKS;
        p = &task[NR_TASKS];
        while (--i) {
            if (!*--p)
                continue;
            if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
                c = (int) (*p)->counter, next = i;
        }
        if (c)
            break;
        for (p = &task[NR_TASKS - 1]; p > &task[0]; --p)
            if (*p)
                (*p)->counter = ((*p)->counter >> 1) +
                                (*p)->priority;
    }
    switch_to(task[next]);
}

#endif

#ifndef SJF
#ifndef PRIORITY
void schedule() {
    printk("not defined");
}
#endif
#endif
