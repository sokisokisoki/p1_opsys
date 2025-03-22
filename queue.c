#include "queue.h"

// Create a new empty queue
Queue* create_queue() {
    Queue *queue = (Queue*)malloc(sizeof(Queue));
    if (queue == NULL) {
        fprintf(stderr, "Memory allocation failed for queue\n");
        exit(EXIT_FAILURE);
    }
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
    return queue;
}

// Add a Process to the rear of the queue
void enqueue(Queue *queue, Process *process) {
    Node *new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) {
        fprintf(stderr, "Memory allocation failed for new node\n");
        exit(EXIT_FAILURE);
    }
    new_node->process = process;
    new_node->next = NULL;

    if (is_empty(queue)) {
        queue->front = new_node;
    } else {
        queue->rear->next = new_node;
    }
    queue->rear = new_node;
    queue->size++;
}

// Remove and return the Process at the front of the queue
Process* dequeue(Queue *queue) {
    if (is_empty(queue)) {
        fprintf(stderr, "Queue is empty, cannot dequeue\n");
        exit(EXIT_FAILURE);
    }

    Node *temp = queue->front;
    Process *process = temp->process;

    queue->front = queue->front->next;
    if (queue->front == NULL) {
        queue->rear = NULL; // Queue is now empty
    }

    free(temp);
    queue->size--;
    return process;
}

// Check if the queue is empty
bool is_empty(Queue *queue) {
    return queue->size == 0;
}

// Get the current size of the queue
int queue_size(Queue *queue) {
    return queue->size;
}

// Free the queue and all its nodes
void free_queue(Queue *queue) {
    while (!is_empty(queue)) {
        dequeue(queue);
    }
    free(queue);
}