#include "printk.h"
#include "sbi.h"
#include "defs.h"

extern void test();

int start_kernel() {
    printk("2023");
    printk(" Hello RISC-V\n");

//    uint64 sstatus;
//    sstatus = csr_read(sstatus);
//    printk("sstatus = %d\n", sstatus);

//    uint64 sscratch;
//    sscratch = csr_read(sscratch);
//    printk("sscratch_0 = %d\n",sscratch);
//
//    uint64 sscratch_0 = 0x12345678;
//    csr_write(sscratch, sscratch_0);
//    sscratch = csr_read(sscratch);
//    printk("sscratch_1 = %d\n",sscratch);

    test(); // DO NOT DELETE !!!

	return 0;
}
