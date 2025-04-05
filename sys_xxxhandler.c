#include "common.h"
#include "syscall.h"
#include "stdio.h"

int __sys_xxxhandler(struct pcb_t *caller, struct sc_regs *regs) {
    /* TODO: implement syscall job */
    // printf("The first system call parameter %d\n", regs->a1);
    int param1 = regs->a1; // first parameter
    int param2 = regs->a2; // second parameter
    int result = param1 + param2;
    
    printf("sys_xxxhandler: Received parameters %d and %d, sum = %d\n", param1, param2, result);
    
    return 0;
}