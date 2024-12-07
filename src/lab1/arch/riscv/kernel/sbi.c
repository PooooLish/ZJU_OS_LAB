#include "types.h"
#include "sbi.h"

struct sbiret sbi_ecall(int ext, int fid, uint64 arg0,
			            uint64 arg1, uint64 arg2,
			            uint64 arg3, uint64 arg4,
			            uint64 arg5)
{
    struct sbiret ret;
    //OpenSBI 的返回结果会存放在寄存器 a0，a1 中,其中a0为error code，a1为返回值
    //我们用 sbiret 来接受这两个返回值
    __asm__ volatile (
            "mv a7, %[ext]\n"
            "mv a6, %[fid]\n"
            "mv a5, %[arg5]\n"
            "mv a4, %[arg4]\n"
            "mv a3, %[arg3]\n"
            "mv a2, %[arg2]\n"
            "mv a1, %[arg1]\n"
            "mv a0, %[arg0]\n"
            //将 ext (Extension ID) 放入寄存器 a7 中，fid (Function ID) 放入寄存器 a6 中
            //将 arg0 ~ arg5 放入寄存器 a0 ~ a5 中
            "ecall\n"   //使用ecall指令 ecall之后系统会进入M模式
            "mv %[error], a0\n"
            "mv %[value], a1\n"
            : [error] "=r" (ret.error), [value] "=r" (ret.value)
    : [ext] "r" (ext), [fid] "r" (fid), [arg0] "r" (arg0),
    [arg1] "r" (arg1), [arg2] "r" (arg2), [arg3] "r" (arg3),
    [arg4] "r" (arg4), [arg5] "r" (arg5)
    : "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
    );
    return ret;
}
