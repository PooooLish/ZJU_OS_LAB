# Lab 6: fork机制

> 非常建议大家先通读整篇实验指导，完成思考题后再动手写代码

## 1. 实验目的
* 为 task 加入 **fork** 机制，能够支持通过 **fork** 创建新的用户态 task 。

## 2. 实验环境 
* Environment in previous labs.

## 3. 背景知识
### 3.1 `fork` 系统调用

`fork` 是 Linux 中的重要系统调用，它的作用是将进行了该系统调用的 task 完整地复制一份，并加入 Ready Queue。这样在下一次调度发生时，调度器就能够发现多了一个 task，从这时候开始，新的 task 就可能被正式从 Ready 调度到 Running，而开始执行了。

* 子 task 和父 task 在不同的内存空间上运行。
* `fork` 成功时，父 task  `返回值为子 task 的 pid`，子 task  `返回值为 0`；`fork` 失败，则父 task  `返回值为 -1`。
* 创建的子 task 需要**深拷贝** `task_struct`，并且调整自己的页表、栈 和 CSR 等信息，同时还需要复制一份在用户态会用到的内存（用户态的栈、程序的代码和数据等），并且将自己伪装成是一个因为调度而加入了 Ready Queue 的普通程序来等待调度。在调度发生时，这个新 task 就像是原本就在等待调度一样，被调度器选择并调度。

### 3.2 `fork` 在 Linux 中的实际应用

Linux 的另一个重要系统调用是 `exec`，它的作用是将进行了该系统调用的 task 换成另一个 task 。这两个系统调用一起，支撑起了 Linux 处理多任务的基础。当我们在 shell 里键入一个程序的目录时，shell（比如 zsh 或 bash）会先进行一次 fork，这时候相当于有两个 shell 正在运行。然后其中的一个 shell 根据 `fork` 的返回值（是否为 0），发现自己和原本的 shell 不同，再调用 `exec` 来把自己给换成另一个程序，这样 shell 外的程序就得以执行了。

## 4 实验步骤

### 4.1 准备工作
* 此次实验基于 lab5 同学所实现的代码进行。
* 从 repo 同步以下文件夹: user 并按照以下步骤将这些文件正确放置。
```
.
└── user
    ├── Makefile
    ├── getpid.c
    ├── link.lds
    ├── printf.c
    ├── start.S
    ├── stddef.h
    ├── stdio.h
    ├── syscall.h
    └── uapp.S
```
* 修改 `task_init` 函数中修改为仅初始化一个 task ，之后其余的 task 均通过 `fork` 创建。

### 4.2 实现 fork()

#### 4.2.1 sys_clone
Fork 在早期的 Linux 中就被指定了名字，叫做 `clone`,
```c
#define SYS_CLONE 220
```
我们在实验原理中说到，fork 的关键在于状态和内存的复制。我们不仅需要完整地**深拷贝**一份页表以及 VMA 中记录的用户态的内存，还需要复制内核态的寄存器状态和内核态的内存。并且在最后，需要将 task “伪装”成是因为调度而进入了 Ready Queue。

回忆一下我们是怎样使用 `task_struct` 的，我们并不是分配了一块刚好大小的空间，而是分配了一整个页，并将页的高处作为了 task 的内核态栈。

```
                    ┌─────────────┐◄─── High Address
                    │             │
                    │    stack    │
                    │             │
                    │             │
              sp ──►├──────┬──────┤
                    │      │      │
                    │      ▼      │
                    │             │
                    │             │
                    │             │
                    │             │
    4KB Page        │             │
                    │             │
                    │             │
                    │             │
                    ├─────────────┤
                    │             │
                    │             │
                    │ task_struct │
                    │             │
                    │             │
                    └─────────────┘◄─── Low Address
```

也就是说，内核态的所有数据，包括了栈、陷入内核态时的寄存器，还有上一次发生调度时，调用 `switch_to` 时的 `thread_struct` 信息，都被存在了这短短的 4K 内存中。这给我们的实现带来了很大的方便，这 4K 空间里的数据就是我们需要的所有所有内核态数据了！（当然如果你没有进行步骤 4.0, 那还需要开一个页并复制一份 `thread_info` 信息）

除了内核态之外，你还需要**深拷贝**一份页表，并遍历页表中映射到 parent task 用户地址空间的页表项（为了减小开销，你需要根据 parent task 的 vmas 来 walk page table），这些应该由 parent task 专有的页，如果已经分配并且映射到 parent task 的地址空间中了，就需要你另外分配空间，并从原来的内存中拷贝数据到新开辟的空间，然后将新开辟的页映射到 child task 的地址空间中。

~~~C
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
~~~



#### 4.2.2 __ret_from_fork

让 fork 出来的 task 被正常调度是本实验**最重要**的部分。

一个程序第一次被调度时，其实是可以选择返回到执行哪一个位置指令的。例如我们当时执行的 `__dummy`, 就替代了正常从 `switch_to` 返回的执行流。这次我们同样使用这个 trick，通过修改 `task_struct->thread.ra`，让程序 `ret` 时，直接跳转到我们设置的 symbol `__ret_from_fork`。 

~~~C
  // 为 child task 指定返回点 __ret_from_fork
    child->thread.ra = &__ret_from_fork;
~~~

我们在 `_traps` 中的 `jal x1, trap_handler` 后面插入一个符号：

```asm
    .global _traps
```

父 task 的返回路径是：`sys_clone->trap_handler->_traps->user program`

新 fork 出来的 子 task 的返回路径是： `sys_clone->trap_handler->_traps->user program`

~~~assembly
 _traps:
    	# -----------
        csrr t0, sscratch
        beq t0, x0, _no_switch
        csrw sscratch, sp
        mv sp, t0

_no_switch:   	
    	# 1. save 32 registers and sepc to stack
    	......
    	
    	# -----------

        # 2. call trap_handler
        csrr a0, scause
        csrr a1, sepc
        mv a2, sp
        jal ra, trap_handler
    .global __ret_from_fork

    	# -----------
__ret_from_fork:
        # 3. restore sepc and 32 registers (x2(sp) should be restore last) from stack
        ......
        
        # -----------
_sret:
        # 4. return from trap
        sret

    	# -----------
~~~

### 4.3 编译及测试

第一个main函数输出：

![image-20240114104752544](C:\Users\MaHong\AppData\Roaming\Typora\typora-user-images\image-20240114104752544.png)

![image-20240114104818587](C:\Users\MaHong\AppData\Roaming\Typora\typora-user-images\image-20240114104818587.png)

第二个main函数输出：

![image-20240114104902807](C:\Users\MaHong\AppData\Roaming\Typora\typora-user-images\image-20240114104902807.png)

![image-20240114104914848](C:\Users\MaHong\AppData\Roaming\Typora\typora-user-images\image-20240114104914848.png)

第三个main函数输出：

![image-20240114104951910](C:\Users\MaHong\AppData\Roaming\Typora\typora-user-images\image-20240114104951910.png)

![image-20240114105009910](C:\Users\MaHong\AppData\Roaming\Typora\typora-user-images\image-20240114105009910.png)

![image-20240114105022237](C:\Users\MaHong\AppData\Roaming\Typora\typora-user-images\image-20240114105022237.png)

## 5 思考题

1. **参考 task_init 创建一个新的 task, 将的 parent task 的整个页复制到新创建的 task_struct 页上, 这一步复制了哪些东西?**

   复制了 `parent task` 中的内核态的所有数据，包括栈，陷入内核状态的寄存器的值，还有调度的 `switch_to , thread_struct ， struct pt_regs` 等信息。

2. **将 thread.ra 设置为 `__ret_from_fork`, 并正确设置 `thread.sp`。仔细想想，这个应该设置成什么值?可以根据 child task 的返回路径来倒推。**

   如代码中所实现的那样，应首先在第一问中所描述的页中，找到 `struct pt_regs` 的地址，也就是让 sp 指 向`pt_regs` 的第一个值（即 `parent task` 陷入内核态的 `sp` )

    `child->thread.sp = (struct pt_regs*)(child + (uint64)regs - PGROUNDDOWN((uint64)regs));`

    在这之后，将` child->thread.sp `所指向的 `struct pt_regs` 中的 `sp` 设置为该段内容的首地址。 

   `((struct pt_regs*)(child->thread.sp))->x[0] = (uint64)child->thread.sp;`

3. **利用参数 regs 来计算出 child task 的对应的 pt_regs 的地址，并将其中的 a0, sp, sepc 设置成正确的值。为什么还要设置 sp?**

   正如第二问中所说，需要将 `parent task` 陷入内核态的寄存器的值 `（ struct pt_regs ）`保留下来，所以 将 `sp` 指向了 `child task` 的该结构的首地址。

   由于 `child task` 返回逻辑是 `__switch_to- >__ret_from_fork(in _traps)->user program` ，没有经历 `parent task` 的 `sys_clone- >trap_handler->_traps->user program` ，因此面向内核时需要保留调度信息。

   

   

   
