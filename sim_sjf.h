#ifndef SIM_SJF_H
#define SIM_SJF_H

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "queue.h"
#include "process.h"

// tau     =  alpha  x  t    +   (1-alpha)  x  tau
//      i+1                i                        i

// Helper function to calculate tau (estimated burst time)
static int calculate_tau(double alpha, int previous_tau, int actual_burst) {
    return (int)ceil((alpha * actual_burst) + ((1 - alpha) * previous_tau));
}

// Modified enqueue function that takes tau array as parameter
static void enqueue_sjf(Queue *queue, Process *process, const int *tau_values, int process_index) {
    Node *new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) {
        fprintf(stderr, "Memory allocation failed for new node\n");
        exit(EXIT_FAILURE);
    }
    new_node->process = process;
    new_node->next = NULL;

    // If queue is empty or new process has smaller tau than front
    if (is_empty(queue) || tau_values[process_index] < tau_values[queue->front->process->index]) {
        new_node->next = queue->front;
        queue->front = new_node;
        if (queue->rear == NULL) queue->rear = new_node;
    } else {
        // Find the correct position to insert
        Node *current = queue->front;
        while (current->next != NULL && tau_values[process_index] >= tau_values[current->next->process->index]) {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
        if (new_node->next == NULL) queue->rear = new_node;
    }
    queue->size++;
}

void simulate_sjf(Process *processes, int n_processes, int tcs, double alpha, double lambda) {
    printf("time 0ms: Simulator started for SJF [Q empty]\n");

    int current_time = 0, finished_processes = 0;
    // First, add an index field to each process
    for (int i = 0; i < n_processes; i++) processes[i].index = i;

    // track remaining CPU bursts
    int *remaining_bursts = (int *)malloc(n_processes * sizeof(int));
    // track burst index per process
    int *burst_index = (int *)calloc(n_processes, sizeof(int));
    // track IO completions
    int *io_completion_time = (int *)calloc(n_processes, sizeof(int));
    // track tau values (estimated burst times)
    int *tau_values = (int *)malloc(n_processes * sizeof(int));
    for (int i = 0; i < n_processes; i++) {
        remaining_bursts[i] = processes[i].num_bursts;
        tau_values[i] = (int)ceil(1.0 / lambda);
    }

    // Stats variables
    int *wait_times = (int *)calloc(n_processes, sizeof(int));
    int *turnaround_times = (int *)calloc(n_processes, sizeof(int));
    int total_context_switches = 0;
    int total_burst_time = 0;
    int total_bursts = 0;

    int cb_context_switches = 0, io_context_switches = 0;
    int cb_count = 0, io_count = 0;
    float cb_wait = 0, io_wait = 0, cb_turn = 0, io_turn = 0;

    // ready queue (will be kept sorted by tau)
    Queue *ready_queue = create_queue();

     // cpu state
    Process *cpu_process = NULL;
    int cpu_idle_until = -1, cpu_burst_end_time = -1;

    while (finished_processes < n_processes) {
        // arrivals
        for (int i = 0; i < n_processes; i++) {
            if (processes[i].arrival_time == current_time) {
                if (current_time < 10000)
                    printf("time %dms: Process %s (tau %dms) arrived; added to ready queue ", current_time, processes[i].id, tau_values[i]);
                enqueue_sjf(ready_queue, &processes[i], tau_values, i);
                if (current_time < 10000) print_queue(ready_queue);
            }
        }

        // IO burst completions
        for (int i = 0; i < n_processes; i++) {
            if (io_completion_time[i] != 0 && io_completion_time[i] == current_time) {
                if (current_time < 10000)
                    printf("time %dms: Process %s (tau %dms) completed I/O; added to ready queue ", current_time, processes[i].id, tau_values[i]);
                enqueue_sjf(ready_queue, &processes[i], tau_values, i);
                if (current_time < 10000) print_queue(ready_queue);
                io_completion_time[i] = 0;
            }
        }

        // cpu burst completions
        if (cpu_process != NULL && current_time == cpu_burst_end_time) {
            int pid = cpu_process->index;
            int old_tau = tau_values[pid];
            int actual_burst = cpu_process->cpu_bursts[burst_index[pid]];
            tau_values[pid] = calculate_tau(alpha, old_tau, actual_burst);

            remaining_bursts[pid]--;
            int bursts_left = remaining_bursts[pid];

            total_burst_time += actual_burst;
            total_bursts++;

            if (current_time < 10000) {
                printf("time %dms: Process %s (tau %dms) completed a CPU burst; %d bursts to go ", current_time, cpu_process->id, old_tau, bursts_left);
                print_queue(ready_queue);
                printf("time %dms: Recalculated tau for process %s: old tau %dms ==> new tau %dms ", current_time, cpu_process->id, old_tau, tau_values[pid]);
                print_queue(ready_queue);
            }

            if (bursts_left > 0) {
                int io_time = cpu_process->io_bursts[burst_index[pid]];
                io_completion_time[pid] = current_time + tcs / 2 + io_time;

                if (current_time < 10000) {
                    printf("time %dms: Process %s switching out of CPU; blocking on I/O until time %dms ", current_time, cpu_process->id, io_completion_time[pid]);
                    print_queue(ready_queue);
                }

                burst_index[pid]++;
            } else {
                turnaround_times[pid] = current_time - processes[pid].arrival_time;
                printf("time %dms: Process %s terminated ", current_time, cpu_process->id);
                print_queue(ready_queue);
                finished_processes++;
            }

            cpu_process = NULL;
            cpu_idle_until = current_time + tcs / 2;
        }

        // start next cpu burst
        if (cpu_process == NULL && !is_empty(ready_queue) && current_time >= cpu_idle_until) {
            cpu_process = dequeue(ready_queue);
            int pid = cpu_process->index;
            int burst_time = cpu_process->cpu_bursts[burst_index[pid]];
            int start_time = current_time + tcs / 2;

            if (current_time < 10000) {
                printf("time %dms: Process %s (tau %dms) started using the CPU for %dms burst ", start_time, cpu_process->id, tau_values[pid], burst_time);
                print_queue(ready_queue);
            }

            cpu_burst_end_time = start_time + burst_time;
            cpu_idle_until = start_time;

            total_context_switches++;
            if (cpu_process->is_cpu_bound) cb_context_switches++;
            else io_context_switches++;

            wait_times[pid] += (start_time - processes[pid].arrival_time);
        }

        current_time++;
    }

    printf("time %dms: Simulator ended for SJF [Q empty]\n\n", current_time+1);

    FILE *f = fopen("simout.txt", "a");
    for (int i = 0; i < n_processes; i++) {
        if (processes[i].is_cpu_bound) {
            cb_count++;
            cb_wait += wait_times[i];
            cb_turn += turnaround_times[i];
        } else {
            io_count++;
            io_wait += wait_times[i];
            io_turn += turnaround_times[i];
        }
    }

    fprintf(f, "Algorithm SJF\n");
    fprintf(f, "-- CPU utilization: %.3f%%\n", 100.0 * total_burst_time / current_time);
    fprintf(f, "-- CPU-bound average wait time: %.3f ms\n", cb_count ? cb_wait / cb_count : 0.0);
    fprintf(f, "-- I/O-bound average wait time: %.3f ms\n", io_count ? io_wait / io_count : 0.0);
    fprintf(f, "-- overall average wait time: %.3f ms\n", (cb_wait + io_wait) / n_processes);
    fprintf(f, "-- CPU-bound average turnaround time: %.3f ms\n", cb_count ? cb_turn / cb_count : 0.0);
    fprintf(f, "-- I/O-bound average turnaround time: %.3f ms\n", io_count ? io_turn / io_count : 0.0);
    fprintf(f, "-- overall average turnaround time: %.3f ms\n", (cb_turn + io_turn) / n_processes);
    fprintf(f, "-- CPU-bound number of context switches: %d\n", cb_context_switches);
    fprintf(f, "-- I/O-bound number of context switches: %d\n", io_context_switches);
    fprintf(f, "-- overall number of context switches: %d\n", total_context_switches);
    fprintf(f, "-- CPU-bound number of preemptions: 0\n");
    fprintf(f, "-- I/O-bound number of preemptions: 0\n");
    fprintf(f, "-- overall number of preemptions: 0\n\n");
    fclose(f);

    // Cleanup
    free(remaining_bursts);
    free(burst_index);
    free(io_completion_time);
    free(tau_values);
    free(wait_times);
    free(turnaround_times);
    free_queue(ready_queue);
}

#endif // SIM_SJF_H
