// arch/riscv/kernel/vm.c
#include "types.h"
#include "defs.h"
#include "printk.h"
#include "vm.h"
#include "mm.h"
#include "string.h"
#include "proc.h"

/* early_pgtbl: 用于 setup_vm 进行 1GB 的 映射。 */
unsigned long early_pgtbl[512] __attribute__((__aligned__(0x1000)));

void setup_vm(void)
{
    /*
    1. 由于是进行 1GB 的映射 这里不需要使用多级页表
    2. 将 va 的 64bit 作为如下划分： | high bit | 9 bit | 30 bit |
        high bit 可以忽略
        中间9 bit 作为 early_pgtbl 的 index
        低 30 bit 作为 页内偏移 这里注意到 30 = 9 + 9 + 12， 即我们只使用根页表， 根页表的每个 entry 都对应 1GB 的区域。
    3. Page Table Entry 的权限 V | R | W | X 位设置为 1
    */

    memset(early_pgtbl, 0x0, PGSIZE);
    unsigned long PA = PHY_START;
    unsigned long VA = PA;
    uint64 index = (VA >> 30) & 0x1ff;
    early_pgtbl[index] = (PA >> 2) | 0xf; // VPN[2] = 2

    VA = VM_START;
    index = (VA >> 30) & 0x1ff;
    early_pgtbl[index] = (PA >> 2) | 0xf; // VPN[2] = 384
}

/* swapper_pg_dir: kernel pagetable 根目录， 在 setup_vm_final 进行映射。 */
unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));
extern uint64 _stext, _srodata, _sdata;

void setup_vm_final(void)
{

    memset(swapper_pg_dir, 0x0, PGSIZE);

    // No OpenSBI mapping required

    // mapping kernel text X|-|R|V
    create_mapping(swapper_pg_dir, (uint64)&_stext, (uint64)(&_stext) - PA2VA_OFFSET, (uint64)(&_srodata) - (uint64)(&_stext), 11);

    // mapping kernel rodata -|-|R|V
    create_mapping(swapper_pg_dir, (uint64)&_srodata, (uint64)(&_srodata) - PA2VA_OFFSET, (uint64)(&_sdata) - (uint64)(&_srodata), 3);

    // mapping other memory -|W|R|V
    create_mapping(swapper_pg_dir, (uint64)&_sdata, (uint64)(&_sdata) - PA2VA_OFFSET, (uint64)(&_stext) + PHY_SIZE - (uint64)(&_sdata), 7);

    // set satp with swapper_pg_dir

    // YOUR CODE HERE
    uint64 tmp_satp = (((uint64)(swapper_pg_dir)-PA2VA_OFFSET) >> 12) | ((uint64)8 << 60);
    csr_write(satp, tmp_satp);
    // printk("set satp to %lx\n", tmp_satp);

    // flush TLB
    asm volatile("sfence.vma zero, zero");

    // flush icache
    asm volatile("fence.i");

    return;
}

/**** 创建多级页表映射关系 *****/
/* 不要修改该接口的参数和返回值 */
create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm)
{
    /*
    pgtbl 为根页表的基地址
    va, pa 为需要映射的虚拟地址、物理地址
    sz 为映射的大小，单位为字节
    perm 为映射的权限 (即页表项的低 8 位)

    创建多级页表的时候可以使用 kalloc() 来获取一页作为页表目录
    可以使用 V bit 来判断页表项是否存在
    */
    // printk("root: %lx, [%lx, %lx) -> [%lx, %lx), perm: %x\n", pgtbl, pa, pa + sz, va, va + sz, perm);
    uint64 *pgtbl1, *pgtbl0;
    for (uint64 i = 0; i < sz; i += PGSIZE)
    {
        uint64 va_i = va + i;
        uint64 pa_i = pa + i;

        uint64 vpn2 = (va_i >> 30) & 0x1ff;
        uint64 vpn1 = (va_i >> 21) & 0x1ff;
        uint64 vpn0 = (va_i >> 12) & 0x1ff;

        if ((pgtbl[vpn2] & 0x1) == 0)
        {
            pgtbl1 = (uint64 *)kalloc();
            pgtbl[vpn2] = ((((uint64)pgtbl1 - PA2VA_OFFSET) >> 12) << 10) | 1;
        }
        pgtbl1 = (uint64 *)(((pgtbl[vpn2] >> 10) << 12) + PA2VA_OFFSET);


        if ((pgtbl1[vpn1] & 0x1) == 0)
        {
            pgtbl0 = (uint64 *)kalloc();
            pgtbl1[vpn1] = ((((uint64)pgtbl0 - PA2VA_OFFSET) >> 12) << 10) | 1;
        }
        pgtbl0 = (uint64 *)(((pgtbl1[vpn1] >>10) << 12) + PA2VA_OFFSET);
        
        pgtbl0[vpn0] = ((pa_i >> 12) << 10) | perm;
    }
}