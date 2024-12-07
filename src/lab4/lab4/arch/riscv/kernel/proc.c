// arch/riscv/kernel/proc.c
#include "proc.h"
#include "mm.h"
#include "defs.h"
#include "rand.h"
#include "printk.h"
#include "test.h"
#include "string.h"
#include "elf.h"

// arch/riscv/kernel/proc.c

extern void __dummy();

struct task_struct *idle;           // idle process
struct task_struct *current;        // 指向当前运行线程的 `task_struct`
struct task_struct *task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

/**
 * new content for unit test of 2023 OS lab2
 */
extern uint64 task_test_priority[]; // test_init 后, 用于初始化 task[i].priority 的数组
extern uint64 task_test_counter[];  // test_init 后, 用于初始化 task[i].counter  的数组
extern unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));
extern create_mapping(pagetable_t, uint64, uint64, uint64, uint64);
extern char _sramdisk[], _eramdisk[];
static uint64_t load_program(struct task_struct *task);

void task_init()
{
    test_init(NR_TASKS);
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle

    /* YOUR CODE HERE */
    idle = (struct task_struct *)kalloc(); // 1
    idle->state = TASK_RUNNING;            // 2
    idle->counter = idle->priority = 0;    // 3
    idle->pid = 0;                         // 4
    current = task[0] = idle;              // 5

    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, 此外，为了单元测试的需要，counter 和 priority 进行如下赋值：
    //      task[i].counter  = task_test_counter[i];
    //      task[i].priority = task_test_priority[i];
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
    // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址,  `sp` 设置为 该线程申请的物理页的高地址

    /* YOUR CODE HERE */
    for (int i = 1; i < NR_TASKS; i++)
    {
        task[i] = (struct task_struct *)kalloc();  // 1
        task[i]->state = TASK_RUNNING;             // 2
        task[i]->counter = task_test_counter[i];   // 2
        task[i]->priority = task_test_priority[i]; // 2
        task[i]->pid = i;

        // Set up thread_struct
        task[i]->thread.ra = (uint64)__dummy;           // 4
        task[i]->thread.sp = (uint64)kalloc() + PGSIZE; // 4

        task[i]->thread.sepc = USER_START;
        uint64 sstatus = csr_read(sstatus);
        sstatus &= ~(1 << 8); // SPP
        sstatus |= (1 << 5);  // SPIE
        sstatus |= (1 << 18); // SUM
        task[i]->thread.sstatus = sstatus;
        task[i]->thread.sscratch = USER_END;

        // Create and set up page table
        task[i]->pgd = (pagetable_t)alloc_page();
        memcpy(task[i]->pgd, swapper_pg_dir, PGSIZE);

        uint64 size = ((uint64)_eramdisk - (uint64)_sramdisk) / PGSIZE+1;
        char *_uapp_start = (char *)alloc_pages(size);
        memcpy(_uapp_start, _sramdisk, (uint64)_eramdisk - (uint64)_sramdisk); // 二进制文件需要先被拷贝到一块某个进程专用的内存之后再进行映射
        create_mapping(task[i]->pgd, USER_START, (uint64)_uapp_start - PA2VA_OFFSET, (uint64)(_eramdisk) - (uint64)(_sramdisk), 0x1F);
        create_mapping(task[i]->pgd, USER_END - PGSIZE, (uint64)alloc_page() - PA2VA_OFFSET, PGSIZE, 0x17);

        // load_program(task[i]);
    }

    printk("...proc_init done!\n");
}

static uint64_t load_program(struct task_struct *task)
{
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)_sramdisk;
    // ELF文件起始地址

    uint64_t phdr_start = (uint64_t)ehdr + ehdr->e_phoff; // 获得phdr起始地址
    int phdr_cnt = ehdr->e_phnum; // 获得段的个数

    Elf64_Phdr *phdr;
    for (int i = 0; i < phdr_cnt; i++)
    {
        phdr = (Elf64_Phdr *)(phdr_start + sizeof(Elf64_Phdr) * i);
        if (phdr->p_type == PT_LOAD) //如果段的type是PT_LOAD则装入内存
        {
            // 将uapp拷贝
            char *_start = (char *)(_sramdisk + phdr->p_offset);
            uint64 size = phdr->p_memsz / PGSIZE + 1;
            uint64 offset = phdr->p_vaddr % PGSIZE;
            char *_uapp = (char *)alloc_pages(size);
            //做地址映射
            memcpy(_uapp + offset, _start, phdr->p_memsz);
            if (phdr->p_memsz > phdr->p_filesz) {
                memset(_uapp + offset + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
            }
            // 映射U-MODE Stack，权限U|X|W|R|V
            create_mapping(task->pgd, phdr->p_vaddr, (uint64)_uapp - PA2VA_OFFSET, phdr->p_memsz + offset, 0x1F);
        }
    }

    // allocate user stack and do mapping

    // following code has been written for you
    // set user stack
    create_mapping(task->pgd, USER_END - PGSIZE, (uint64)alloc_page() - PA2VA_OFFSET, PGSIZE, 0x17);
    // pc for the user program
    task->thread.sepc = ehdr->e_entry;
    // sstatus bits set
    uint64 sstatus = csr_read(sstatus);
    sstatus &= ~(1 << 8); // SPP
    sstatus |= (1 << 5);  // SPIE
    sstatus |= (1 << 18); // SUM
    task->thread.sstatus = sstatus;
    // user stack for user program
    task->thread.sscratch = USER_END;
}

// arch/riscv/kernel/proc.c
void dummy()
{
    schedule_test();
    uint64 MOD = 1000000007;
    uint64 auto_inc_local_var = 0;
    int last_counter = -1;
    while (1)
    {
        if ((last_counter == -1 || current->counter != last_counter) && current->counter > 0)
        {
            if (current->counter == 1) {
                --(current->counter); // forced the counter to be zero if this thread is going to be scheduled
            }                         // in case that the new counter is also 1, leading the information not printed.
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. auto_inc_local_var = %d. thread space begin at 0x%lx\n", current->pid, auto_inc_local_var, current->thread.sp);
        }
    }
}

extern void __switch_to(struct task_struct *prev, struct task_struct *next);

void switch_to(struct task_struct *next)
{
    /* YOUR CODE HERE */
    if (current != next)
    {
        // printk("switch from [PID = %d] to [PID = %d]\n", current->pid, next->pid);
        struct task_struct *prev = current;
        current = next;
        __switch_to(prev, next);
    }
}

void do_timer(void)
{
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行调度

    /* YOUR CODE HERE */
    // printk("do_timer\n");
    if (current == idle) // 1
    {
        // printk("current==idle\n");
        schedule();
    }
    else // 2
    {
        // printk("current->counter = %d\n", current->counter);
        if (current->counter > 0)
        {
            current->counter--;
        }
        if (current->counter == 0)
        {
            schedule();
        }
    }
}

#ifdef SJF
void schedule(void)
{
    /* YOUR CODE HERE */
    int next = 1;
    int min = 0x7fffffff;
    int all_zero = 1;
    for (int i = 1; i < NR_TASKS; i++)
    {
        if (task[i]->state == TASK_RUNNING && task[i]->counter > 0)
        {
            if (task[i]->counter < min)
            {
                next = i;
                min = task[i]->counter;
            }
            all_zero = 0;
        }
    }
    if (all_zero)
    {
        // printk("all zero\n");
        printk("\n");
        for (int i = 1; i < NR_TASKS; i++)
        {
            task[i]->counter = rand();
            printk("SET [PID = %d COUNTER = %d]\n", task[i]->pid, task[i]->counter);
        }
        schedule();
    }
    else
    {
        // printk("next = %d\n",next);
        printk("\n");
        if (next)
        {
            printk("switch to [PID = %d COUNTER = %d]\n", task[next]->pid, task[next]->counter);
            switch_to(task[next]);
        }
    }
}
#endif

#ifdef PRIORITY
void schedule(void)
{
    /* YOUR CODE HERE */
    int next = 1;
    int max = 0;
    int all_zero = 1;
    for (int i = 1; i < NR_TASKS; i++)
    {
        if (task[i]->state == TASK_RUNNING && task[i]->counter > 0)
        {
            if (task[i]->counter > max)
            {
                next = i;
                max = task[i]->counter;
            }
            all_zero = 0;
        }
    }
    if (all_zero)
    {
        printk("\n");
        for (int i = 1; i < NR_TASKS; i++)
        {
            task[i]->counter = (task[i]->counter >> 1) + task[i]->priority;
            printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", task[i]->pid, task[i]->priority, task[i]->counter);
        }
        schedule();
    }
    else
    {
        printk("\n");
        if (next)
        {
            printk("switch to [PID = %d PRIORITY = %d COUNTER = %d]\n", task[next]->pid, task[next]->priority, task[next]->counter);
            switch_to(task[next]);
        }
    }
}
#endif