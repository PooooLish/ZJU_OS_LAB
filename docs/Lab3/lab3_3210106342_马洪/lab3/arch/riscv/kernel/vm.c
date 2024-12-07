// arch/riscv/kernel/vm.c
#include "defs.h"
#include "types.h"
#include "vm.h"
#include "mm.h"
#include "string.h"
#include "printk.h"

/* early_pgtbl: 用于 setup_vm 进行 1GB 的 映射。 */
unsigned long early_pgtbl[512] __attribute__((__aligned__(0x1000)));
unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));


void setup_vm(void) {
    /*
    1. 由于是进行 1GB 的映射 这里不需要使用多级页表
    2. 将 va 的 64bit 作为如下划分： | high bit | 9 bit | 30 bit |
        high bit 可以忽略
        中间9 bit 作为 early_pgtbl 的 index
        低 30 bit 作为 页内偏移 这里注意到 30 = 9 + 9 + 12， 即我们只使用根页表， 根页表的每个 entry 都对应 1GB 的区域。
    3. Page Table Entry 的权限 V | R | W | X 位设置为 1
    */

    memset(early_pgtbl, 0, PGSIZE);
    memset(swapper_pg_dir, 0, PGSIZE);

    // 第一次映射，等值映射
    int vpn, ppn;
    // 这里是根页表，所以右移 30 位，即 1G 空间
    vpn = (PHY_START >> 30);
    ppn = (PHY_START >> 30) & 0x3ffffff;
    // 将顶级页表的权限位低 4 位设置为 1111
    early_pgtbl[vpn] = (ppn<<28) + 15;


    // 第二次映射，映射到direct mapping area
    vpn = (VM_START >> 30) & 0x1ff;
    early_pgtbl[vpn] = (ppn<<28) + 15;

}

/* swapper_pg_dir: kernel pagetable 根目录， 在 setup_vm_final 进行映射。 */

extern char _srodata[];
extern char _stext[];
extern char _sdata[];

void setup_vm_final(void) {

    // 获得 kernel 的 text 段，data 段，以及 user 段的空间大小
    int size_kt, size_kd, size_ot;
    size_kt = (uint64)_srodata-(uint64)_stext;
    size_kd = (uint64)_sdata-(uint64)_srodata;
    size_ot = PHY_SIZE - ((uint64)_sdata-(uint64)_stext);

    // 存储每一对需要映射的虚拟地址和物理地址
    uint64 pa, va;

    // mapping kernel text X|-|R|V
    pa = PHY_START + OPENSBI_SIZE;
    va = VM_START  + OPENSBI_SIZE;
    // 权限位为 1011
    create_mapping(swapper_pg_dir, va, pa, size_kt, 0b1011);

    // mapping kernel data -|-|R|V
    pa += size_kt;
    va += size_kt;
    // 权限位为 0011
    create_mapping(swapper_pg_dir, va, pa, size_kd, 0b0011);

    // mapping other memory -|W|R|V
    pa += size_kd;
    va += size_kd;
    // 权限位为 0111
    create_mapping(swapper_pg_dir, va, pa, size_ot, 0b0111);

    // set satp with swapper_pg_dir
    // Mode 位置为 8，在高 4 位
    // 将 swapper_pg_dir 顶级页表地址放在 satp 寄存器中
    asm volatile (
            "li t0, 8\n"
            "slli t0, t0, 60\n"
            "mv t1, %[pgt]\n"
            "srli t1, t1, 12\n"
            "add t0, t0, t1\n"
            "csrw satp, t0"
            :
            :[pgt] "r" ((uint64)swapper_pg_dir - PA2VA_OFFSET)
    :"memory"
    );

    // flush TLB
    asm volatile("sfence.vma zero, zero");
    // flush icache
    asm volatile("fence.i");

    return;
}

/* 创建多级页表映射关系 */
/*
    pgtbl 为根页表的基地址
    va, pa 为需要映射的虚拟地址、物理地址
    sz 为映射的大小 (单位为4KB)
    perm 为映射的读写权限
*/
void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm) {
    /*
    pgtbl 为根页表的基地址
    va, pa 为需要映射的虚拟地址、物理地址
    sz 为映射的大小
    perm 为映射的读写权限

    创建多级页表的时候可以使用 kalloc() 来获取一页作为页表目录
    可以使用 V bit 来判断页表项是否存在
    */

    uint64 *tbl[2]; // 存储一、二级页表
    uint64 PTE[2]; // 一、二级的页表项
    uint64 *page_ = NULL; // 存储新分配页的地址
    int v1, v2, v3; // 存储虚拟地址的高，中，低 9 位
    uint64 offset = 0;


    for(uint64 va_ = va; va_ < sz + va; va_ += PGSIZE) {

        // 根据 Sv39 的虚拟地址映射规则，虚拟地址去掉 12 位偏移量后，高，中，低 9 位为三级页表的索引
        v1 = (va_ >> 30) & 0x1ff;
        v2 = (va_ >> 21) & 0x1ff;
        v3 = (va_ >> 12) & 0x1ff;

        // 填充第一级页表
        PTE[1] = pgtbl[v1];

        if((PTE[1] & 1) == 0) {
            page_ = (uint64*)kalloc();

            PTE[1] = ((((uint64)page_ - PA2VA_OFFSET) >> 12) << 10) + 1;
            pgtbl[v1] = PTE[1];
        }

        // 创建并填充第二级页表
        tbl[1] = (uint64*)((PTE[1] >> 10) << 12);

        PTE[0] = tbl[1][v2];
        if((PTE[0] & 1) == 0) {
            page_ = (uint64*)kalloc();
            PTE[0] = ((((uint64)page_ - PA2VA_OFFSET) >> 12) << 10) + 1;
            tbl[1][v2] = PTE[0];
        }

        // 填充第三级页表
        tbl[0] = (uint64*)((PTE[0] >> 10) << 12);

        // 添加页表的权限，直接放置在低 8 位
        tbl[0][v3] = (((pa + offset) >> 12) << 10) + perm;
        offset += PGSIZE;

    }

    return;
}