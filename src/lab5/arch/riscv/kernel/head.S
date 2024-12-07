//arch/riscv/kernel/head.S
.extern start_kernel

    .section .text.init
    .globl _start
_start:
    # ------------------
    la sp, boot_stack_top
    li t0, 0xffffffdf80000000
    sub sp, sp, t0

    call setup_vm
    call relocate

    call mm_init

    call setup_vm_final

    call task_init
    
    # set stvec = _traps
    la t0, _traps
    csrw stvec, t0 

    # set sie[STIE] = 1
    li t0, 1 << 5 
    csrw sie, t0

    # set first time interrupt
    call clock_set_next_event

    # set sstatus[SIE] = 1
    # li t0, 1 << 1
    # csrw sstatus, t0

    # la sp, boot_stack_top
    call start_kernel
    # ------------------

relocate:
    # set ra = ra + PA2VA_OFFSET
    # set sp = sp + PA2VA_OFFSET (If you have set the sp before)
    li t0, 0xffffffdf80000000
    add ra, ra, t0
    add sp, sp, t0

    ###################### 
    #   YOUR CODE HERE   #
    ######################

    # set satp with early_pgtbl
    la t1, early_pgtbl
    sub t1, t1, t0 
    srli t1, t1, 12
    li t0, 0x8
    slli t0, t0, 60
    or t1, t1, t0
    csrw satp, t1

    ###################### 
    #   YOUR CODE HERE   #
    ######################

    # flush tlb
    sfence.vma zero, zero

    # flush icache
    fence.i

    ret

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 # <-- change to your stack size

    .globl boot_stack_top
boot_stack_top: