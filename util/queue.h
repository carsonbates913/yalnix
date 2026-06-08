#ifndef QUEUE_H
#define QUEUE_H

struct pcb;
typedef struct pcb pcb_t;

typedef struct process_queue {
    pcb_t *head;
    pcb_t *tail;
} process_queue_t;

void queue_init(process_queue_t *q);
void enqueue(process_queue_t *q, pcb_t *pcb);
pcb_t *dequeue(process_queue_t *q);
int queue_is_empty(process_queue_t *q);

#endif