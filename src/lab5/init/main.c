#include "printk.h"
#include "sbi.h"
#include "defs.h"

extern void test();

int start_kernel()
{
    printk("2023");
    printk(" Hello RISC-V\n");
    printk("idle process is running!\n");
    schedule();
    
    /* lab3 */
    // extern char _stext[];
    // extern char _srodata[];
    // printk("_stext = %lx\n", *_stext);
    // printk("_srodata = %lx\n", *_srodata);
    // *_stext = 0;
    // *_srodata = 0;
    // printk("_stext = %lx\n", *_stext);
    // printk("_srodata = %lx\n", *_srodata);

    /* lab1 */
    // sbi_ecall(0x1, 0x0, 0x30, 0, 0, 0, 0, 0);
    // printk("\n");
    // uint64 a=csr_read(sstatus);
    // printk("sstatus= %lx\n", a);
    // csr_write(sscratch, 1000);
    // uint64 b=csr_read(sscratch);
    // printk("sscratch: %d\n", b);

    test(); // DO NOT DELETE !!!

    return 0;
}
