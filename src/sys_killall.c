#include "common.h"
#include "syscall.h"
#include "stdio.h"
#include "libmem.h"
#include "string.h"   // for strcmp
#include "queue.h" //Include queue.h for queue operations

/* Helper: Remove and “kill” (simulate termination of) all processes
   with matching name in the given queue. */
static void kill_all_in_queue(struct queue_t *q, const char *target_name) {
    if (q == NULL) return;
    
    /* Instead of declaring a struct queue_t (which is incomplete here)
     * we create a temporary queue using an anonymous struct with the same fields.
     */
    struct {
        struct pcb_t *proc[MAX_QUEUE_SIZE];
        int size;
    } tempQueue = { .size = 0 };
    
    // Dequeue each process; if its name (stored in path) matches, simulate termination.
    while (!empty(q)) {
        struct pcb_t *proc = dequeue(q);
        if (strcmp(proc->path, target_name) == 0) {
            printf("Terminating process PID %d with name \"%s\"\n", proc->pid, proc->path);
            // Here you might mark the process as terminated,
            // free its resources, and remove it from other lists.
        } else {
            enqueue((struct queue_t *)&tempQueue, proc);
        }
    }
    
    // Restore surviving processes back to the original queue.
    while (!empty((struct queue_t *)&tempQueue)) {
        enqueue(q, dequeue((struct queue_t *)&tempQueue));
    }
}

int __sys_killall(struct pcb_t *caller, struct sc_regs* regs)
{
    char proc_name[100];
    uint32_t data;
    int i = 0;
    uint32_t memrg = regs->a1;  // hardcode for demo only

    /* Get name of the target proc from the given memory region
     * We assume that libread returns -1 in [data] when end-of-string is reached.
     */
    while (1) {
        data = 0;
        libread(caller, memrg, i, &data);
        if (data == (uint32_t)-1) { 
            proc_name[i] = '\0';
            break;
        }
        proc_name[i] = (char)data;
        i++;
        if (i >= (sizeof(proc_name) - 1)) {
            proc_name[i] = '\0';
            break;
        }
    }
    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    /* Traverse process lists to terminate the processes with matching name.
     * Here we assume that the caller (likely a kernel process) holds pointers to:
     *  - running_list: a queue of processes currently running.
     *  - mlq_ready_queue: if MLQ_SCHED is defined, an array (or pointer) of queues indexed by priority.
     */
    printf("Searching running_list for processes to kill...\n");
    if (caller->running_list != NULL) {
        kill_all_in_queue(caller->running_list, proc_name);
    }
#ifdef MLQ_SCHED
    printf("Searching mlq_ready_queue for processes to kill...\n");
    if (caller->mlq_ready_queue != NULL) {
        // For this demo, assume mlq_ready_queue is a single queue.
        // (In a real MLQ scheduler, it may be an array of queues.)
        kill_all_in_queue(caller->mlq_ready_queue, proc_name);
    }
#endif

    return 0; 
}