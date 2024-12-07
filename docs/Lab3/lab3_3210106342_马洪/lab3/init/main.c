#include "printk.h"
#include "sbi.h"
#include "defs.h"

extern void test();

extern char _stext[];
extern char _sdata[];
extern char _srodata[];

int start_kernel() {
    printk("2023 Hello RISC-V\n");

    printk("_stext = %ld\n", *_stext);
    printk("_sdata = %ld\n", *_sdata);
    printk("_srodata = %ld\n", *_srodata);

    *_stext = 0;
    *_sdata = 0;

    test(); // DO NOT DELETE !!!

	return 0;
}
