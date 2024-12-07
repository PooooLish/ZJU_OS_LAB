    .section .text.entry
    .align 2
    .globl _traps 
_traps:
    # YOUR CODE HERE
    # -----------
        csrr t0, sscratch
        beq t0, x0, _no_switch
        csrw sscratch, sp
        mv sp, t0

_no_switch:
        # 1. save 32 registers and sepc to stack
        addi sp, sp, -37*8
        sd zero, 0(sp)
        sd ra, 8(sp)
        sd sp, 16(sp)
        sd gp, 24(sp)
        sd tp, 32(sp)
        sd t0, 40(sp)
        sd t1, 48(sp)
        sd t2, 56(sp)
        sd s0, 64(sp)
        sd s1, 72(sp)
        sd a0, 80(sp)
        sd a1, 88(sp)
        sd a2, 96(sp)
        sd a3, 104(sp)
        sd a4, 112(sp)
        sd a5, 120(sp)
        sd a6, 128(sp)
        sd a7, 136(sp)
        sd s2, 144(sp)
        sd s3, 152(sp)
        sd s4, 160(sp)
        sd s5, 168(sp)
        sd s6, 176(sp)
        sd s7, 184(sp)
        sd s8, 192(sp)
        sd s9, 200(sp)
        sd s10, 208(sp)
        sd s11, 216(sp)
        sd t3, 224(sp)
        sd t4, 232(sp)
        sd t5, 240(sp)
        sd t6, 248(sp)
        csrr t0, sepc
        sd t0, 256(sp)
        csrr t0, sstatus
        sd t0, 264(sp)
        csrr t0, stval
        sd t0, 272(sp)
        csrr t0, sscratch
        sd t0, 280(sp)
        csrr t0, scause
        sd t0, 288(sp)

    # -----------

        # 2. call trap_handler
        csrr a0, scause
        csrr a1, sepc
        mv a2, sp
        jal ra, trap_handler

    # -----------

        # 3. restore sepc and 32 registers (x2(sp) should be restore last) from stack
        ld t0, 288(sp)
        csrw scause, t0
        ld t0, 280(sp)
        csrw sscratch, t0
        ld t0, 272(sp)
        csrw stval, t0
        ld t0, 264(sp)
        csrw sstatus, t0
        ld t0, 256(sp)
        csrw sepc, t0
        ld zero, 0(sp)
        ld ra, 8(sp)
        ld gp, 24(sp)
        ld tp, 32(sp)
        ld t0, 40(sp)
        ld t1, 48(sp)
        ld t2, 56(sp)
        ld s0, 64(sp)
        ld s1, 72(sp)
        ld a0, 80(sp)
        ld a1, 88(sp)
        ld a2, 96(sp)
        ld a3, 104(sp)
        ld a4, 112(sp)
        ld a5, 120(sp)
        ld a6, 128(sp)
        ld a7, 136(sp)
        ld s2, 144(sp)
        ld s3, 152(sp)
        ld s4, 160(sp)
        ld s5, 168(sp)
        ld s6, 176(sp)
        ld s7, 184(sp)
        ld s8, 192(sp)
        ld s9, 200(sp)
        ld s10, 208(sp)
        ld s11, 216(sp)
        ld t3, 224(sp)
        ld t4, 232(sp)
        ld t5, 240(sp)
        ld t6, 248(sp)
        ld sp, 16(sp)
        addi sp, sp, 37*8

        csrr t0, sscratch
        beq t0, x0, _sret
        csrw sscratch, sp
        mv sp, t0

    # -----------
_sret:
        # 4. return from trap
        sret

    # -----------

    .global __dummy
__dummy:
    # YOUR CODE HERE
    csrr t0, sscratch
    csrw sscratch, sp
    mv sp, t0
    sret

    .globl __switch_to
__switch_to:
    # save state to prev process
    # YOUR CODE HERE
    add t0, a0, 48
    sd ra, 0(t0)
    sd sp, 8(t0)
    sd s0, 16(t0)
    sd s1, 24(t0)
    sd s2, 32(t0)
    sd s3, 40(t0)
    sd s4, 48(t0)
    sd s5, 56(t0)
    sd s6, 64(t0)
    sd s7, 72(t0)
    sd s8, 80(t0)
    sd s9, 88(t0)
    sd s10, 96(t0)
    sd s11, 104(t0)
    csrr t1, sepc
    sd t1, 112(t0)
    csrr t1, sstatus
    sd t1, 120(t0)
    csrr t1, sscratch
    sd t1, 128(t0)
    # csrr t1, satp
    # sd t1, 136(t0)

    # restore state from next process
    # YOUR CODE HERE
    add t0, a1, 48
    ld ra, 0(t0)
    ld sp, 8(t0)
    ld s0, 16(t0)
    ld s1, 24(t0)
    ld s2, 32(t0)
    ld s3, 40(t0)
    ld s4, 48(t0)
    ld s5, 56(t0)
    ld s6, 64(t0)
    ld s7, 72(t0)
    ld s8, 80(t0)
    ld s9, 88(t0)
    ld s10, 96(t0)
    ld s11, 104(t0)
    ld t1, 112(t0)
    csrw sepc, t1
    ld t1, 120(t0)
    csrw sstatus, t1
    ld t1, 128(t0)
    csrw sscratch, t1
    # ld t1, 136(t0)
    # csrw satp, t1

    ld t1, 136(t0)
    la t2, PA2VA_OFFSET
    sub t1, t1, t2
    srli t1, t1, 12
    li t2, 0x8
    slli t2, t2, 60
    or t1, t1, t2
    csrw satp, t1

    sfence.vma zero, zero
    fence.i

    ret