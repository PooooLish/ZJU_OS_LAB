#include "proc.h"
#include "mm.h"
#include "rand.h"
#include "printk.h"
#include "defs.h"

extern void __dummy();

struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

extern uint64 task_test_priority[]; // test_init 后，用于初始化 task[i].priority 的数组
extern uint64 task_test_counter[];  // test_init 后，用于初始化 task[i].counter  的数组

void task_init() {
    mm_init();
    test_init(NR_TASKS);

    uint64 addr_idle = kalloc(); // 调用 kalloc() 为 idle 分配一个物理页
    idle = (struct task_struct*)addr_idle;
    idle->state = TASK_RUNNING; // 设置 state 为 TASK_RUNNING;
    idle->counter = idle->priority = 0;	//由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    idle->pid = 0; // 设置 idle 的 pid 为 0
    current = task[0] = idle; //将 current 和 task[0] 指向 idle

    /* YOUR CODE HERE */
    for(int i = 1; i < NR_TASKS; i++){ // 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
        uint64 task_addr = kalloc(); // 调用 kalloc() 为 task[i] 分配一个物理页
        task[i] = (struct task_struct*)task_addr; // 设置 state 为 TASK_RUNNING;
        task[i]->state = TASK_RUNNING; // 设置 state 为 TASK_RUNNING;
        task[i]->counter = task_test_counter[i]; // 设置 counter;
        task[i]->priority = task_test_priority[i]; // 设置 priority;
        task[i]->pid = i; // 设置pid，pid 为该线程在线程数组中的下标
        task[i]->thread.ra = (uint64)__dummy;
        task[i]->thread.sp = task_addr + 4096;
        // 为 task[i] 设置 `thread_struct` 中的 `ra` 和 `sp`,
        // 其中 `ra` 设置为 __dummy （见 4.3.2）的地址,  `sp` 设置为 该线程申请的物理页的高地址
    }

    printk("...proc_init done!\n");
}

void dummy() {
//    schedule_test();
    uint64 MOD = 1000000007;
    uint64 auto_inc_local_var = 0;
    int last_counter = -1;
    while(1) {
        if ((last_counter == -1 || current->counter != last_counter) && current->counter > 0) {
            if(current->counter == 1){
                --(current->counter);   // forced the counter to be zero if this thread is going to be scheduled
            }                           // in case that the new counter is also 1，leading the information not printed.
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. auto_inc_local_var = %d\n", current->pid, auto_inc_local_var);
        }
    }
}
extern void __switch_to(struct task_struct* prev, struct task_struct* next);

void switch_to(struct task_struct* next) {
    if (next == current) return; // 当线程转换的目标是本身的线程时，无需进行操作
    else {
        struct task_struct*  temp= current;
        current = next;
        __switch_to(temp, next);
    }
}

void do_timer(void) {
    if (current == task[0]) schedule();
        // 如果当前线程是 idle 线程,直接进行调度,无需其他处理
    else {
        // 如果当前线程不是 idle，则当前线程的运行剩余时间减1；剩余时间大于0则无需调度
        current->counter -= 1;
        if (current->counter == 0) schedule();
    }
}

// 短作业优先调度算法
#ifdef DSJF
void schedule(void){

    uint64 min = INF; //  将min设为def.h中定义的最大值
    struct task_struct* next = NULL;
    int flag = 1; // 使用flag来检测目前是否还存在需要调度的进程
    for(int i = 1; i < NR_TASKS; i++){
        if (task[i]->state == TASK_RUNNING && task[i]->counter > 0) { // 遍历线程指针数组
            flag = 0; // 当还存在需要调度的进程时，flag = 0
            if (task[i]->counter < min) { //找到所有运行状态下的线程中，运行剩余时间最少的线程，作为下一个执行的线程
                min = task[i]->counter;
                next = task[i];
            }
        }
    }

    if (flag) { // 当目前已经不存在需要调度的进程时，即所有运行状态下的线程运行剩余时间都为0
        printk("\n");
        for(int i = 1; i < NR_TASKS; i++){
            // 对task[1]~task[NR_TASKS-1]的运行剩余时间重新赋值
            task[i]->counter = rand() % 10 + 1;
            printk("SET [PID = %d COUNTER = %d]\n", task[i]->pid, task[i]->counter);
        }
        // 重新进行调度
        schedule();
    }
    else {
        // 转移到需要执行的进程
        if (next) {
            printk("\nswitch to [PID = %d COUNTER = %d]\n", next->pid, next->counter);
            switch_to(next);
        }
    }
}
#endif

// 优先级调度算法
#ifdef DPRIORITY
void schedule(void){
    uint64 c, i, next;
    struct task_struct ** p;

	while(1) {
		c = 0;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
        // 遍历任务列表，找到下一个要运行的任务
		while(--i) {
			if (!*--p) continue;
            // 如果任务的状态为运行状态且时间片大于 c，则更新 c 和下一个任务的索引
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter, next = i;
		}

		if (c) break; // 如果找到了下一个任务，跳出循环

        printk("\n");
        // 如果没有找到可运行的任务，进行时间片重分配
        for(p = &task[NR_TASKS-1]; p > &task[0]; --p) {
            if (*p) {
                // 将任务的时间片减少，再加上任务的优先级
                (*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
                printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", (*p)->pid, (*p)->priority, (*p)->counter);
            }
        }
	}

    printk("\nswitch to [PID = %d PRIORITY = %d COUNTER = %d]\n", task[next]->pid, task[next]->priority, task[next]->counter);
	// 切换到下一个任务
    switch_to(task[next]);
}
#endif