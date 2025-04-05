#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
    if (q == NULL) return 1;
    return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
    // If queue is full, we simply ignore the enqueue request
    if (q->size >= MAX_QUEUE_SIZE) {
        // Optionally print a message or handle error
        return;
    }
    q->proc[q->size] = proc;
    q->size++;
}

struct pcb_t * dequeue(struct queue_t * q) {
    if (empty(q))
        return NULL;

    // Find the process with the highest priority:
    // We assume higher numeric value means higher priority.
    int best_index = 0;
    int i;
    for (i = 1; i < q->size; i++) {
        if (q->proc[i]->priority > q->proc[best_index]->priority)
            best_index = i;
    }

    // Save the selected process
    struct pcb_t *selected = q->proc[best_index];

    // Remove the process from the queue (shift all remaining items)
    for (i = best_index; i < q->size - 1; i++) {
        q->proc[i] = q->proc[i+1];
    }
    q->size--;

    return selected;
}