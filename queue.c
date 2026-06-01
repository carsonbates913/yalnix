#include "kernelstart.h"
#include "queue.h"
#include <stddef.h>

void queue_init(process_queue_t *q) {
    q->head = NULL;
    q->tail = NULL;
}

void enqueue(process_queue_t *q, pcb_t *pcb) {
    pcb->next = NULL;
    if (q->tail == NULL) {
        q->head = pcb;
        q->tail = pcb;
    } else {
        q->tail->next = pcb;
        q->tail = pcb;
    }
}

pcb_t *dequeue(process_queue_t *q) {
    if (q->head == NULL) {
        return NULL;
    }
    pcb_t *pcb = q->head;
    q->head = pcb->next;
    if (q->head == NULL) {
        q->tail = NULL;
    }
    pcb->next = NULL;
    return pcb;
}

int queue_is_empty(process_queue_t *q) {
    return q->head == NULL;
}