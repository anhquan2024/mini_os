#include "queue.h"
#include "sched.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

static struct queue_t running_list;
#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
    unsigned long prio;
    for (prio = 0; prio < MAX_PRIO; prio++)
        if (!empty(&mlq_ready_queue[prio]))
            return -1;
#endif
    return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i;
    for (i = 0; i < MAX_PRIO; i++) {
        mlq_ready_queue[i].size = 0;
        slot[i] = MAX_PRIO - i;
    }
#endif
    ready_queue.size = 0;
    run_queue.size = 0;
    running_list.size = 0;
    pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
/* 
 * get_mlq_proc - get a process from the MLQ ready queue.
 * It searches through the mlq_ready_queue array starting from the highest priority.
 * Returns the selected process, or NULL if none exist.
 */
struct pcb_t * get_mlq_proc(void) {
    struct pcb_t *proc = NULL;
    pthread_mutex_lock(&queue_lock);
    int prio;
    // Iterate from highest priority (MAX_PRIO-1) down to 0.
    for (prio = MAX_PRIO - 1; prio >= 0; prio--) {
        if (!empty(&mlq_ready_queue[prio])) {
            proc = dequeue(&mlq_ready_queue[prio]);
            break;
        }
    }
    pthread_mutex_unlock(&queue_lock);
    return proc;
}

void put_mlq_proc(struct pcb_t * proc) {
    pthread_mutex_lock(&queue_lock);
    enqueue(&mlq_ready_queue[proc->prio], proc);
    pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
    pthread_mutex_lock(&queue_lock);
    enqueue(&mlq_ready_queue[proc->prio], proc);
    pthread_mutex_unlock(&queue_lock);
}

struct pcb_t * get_proc(void) {
    return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
    proc->ready_queue = &ready_queue;
    proc->mlq_ready_queue = mlq_ready_queue;
    proc->running_list = &running_list;
    
    /* Put running process to running_list */
    pthread_mutex_lock(&queue_lock);
    enqueue(&running_list, proc);
    pthread_mutex_unlock(&queue_lock);
    
    put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
    proc->ready_queue = &ready_queue;
    proc->mlq_ready_queue = mlq_ready_queue;
    proc->running_list = &running_list;
    
    /* Put new process to running_list */
    pthread_mutex_lock(&queue_lock);
    enqueue(&running_list, proc);
    pthread_mutex_unlock(&queue_lock);
    
    add_mlq_proc(proc);
}
#else
struct pcb_t * get_proc(void) {
    struct pcb_t * proc = NULL;
    pthread_mutex_lock(&queue_lock);
    if (!empty(&ready_queue))
        proc = dequeue(&ready_queue);
    pthread_mutex_unlock(&queue_lock);
    return proc;
}

void put_proc(struct pcb_t * proc) {
    proc->ready_queue = &ready_queue;
    proc->running_list = &running_list;
    
    /* Put running process to running_list */
    pthread_mutex_lock(&queue_lock);
    enqueue(&running_list, proc);
    enqueue(&run_queue, proc);
    pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
    proc->ready_queue = &ready_queue;
    proc->running_list = &running_list;
    
    /* Put new process to running_list */
    pthread_mutex_lock(&queue_lock);
    enqueue(&running_list, proc);
    enqueue(&ready_queue, proc);
    pthread_mutex_unlock(&queue_lock);
}
#endif