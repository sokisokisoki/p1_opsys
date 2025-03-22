#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include <stdbool.h>

// Forward declaration of the Process struct (assuming it's defined elsewhere)
typedef struct Process Process;

// Node structure for the queue
typedef struct Node {
    Process *process;       // Pointer to the Process struct
    struct Node *next;      // Pointer to the next node in the queue
} Node;

// Queue structure
typedef struct {
    Node *front;            // Pointer to the front of the queue
    Node *rear;             // Pointer to the rear of the queue
    int size;               // Current size of the queue
} Queue;

// Function prototypes
Queue* create_queue();
void enqueue(Queue *queue, Process *process);
Process* dequeue(Queue *queue);
bool is_empty(Queue *queue);
int queue_size(Queue *queue);
void free_queue(Queue *queue);

#endif // QUEUE_H