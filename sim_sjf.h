#ifndef SIM_SJF_H
#define SIM_SJF_H

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "queue.h"
#include "process.h"

// Helper function to calculate tau (estimated burst time)
static int calculate_tau2(double alpha, int previous_tau, int actual_burst) {
    return (alpha == -1) ? actual_burst : (int)ceil((alpha * actual_burst) + ((1 - alpha) * previous_tau));
}

static const char* format_tau(int tau_value) {
    static char buffer[32];  // Static buffer for string construction
    
    if (tau_value == 0) {
        return "";  // No tau display when alpha is -1
    }
    
    snprintf(buffer, sizeof(buffer), "(tau %dms)", tau_value);
    return buffer;
}

// Get process array index from process ID
static int get_process_index(Process *processes, int n_processes, const char *id) {
    for (int i = 0; i < n_processes; i++) {
        if (strcmp(processes[i].id, id) == 0) {
            return i;
        }
    }
    return -1; // Not found
}

// Modified enqueue function for SJF scheduling
static void enqueue_sjf(Queue *queue, Process *process, Process *processes, int n_processes, const int *tau_values, double alpha) {
    Node *new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) {
        fprintf(stderr, "ERROR: Memory allocation failed for new node\n");
        exit(EXIT_FAILURE);
    }
    new_node->process = process;
    new_node->next = NULL;

    int process_index = get_process_index(processes, n_processes, process->id);
    // Get the appropriate value for comparison
    int compare_value = (alpha == -1) ? process->cpu_bursts[process->index] : tau_values[process_index];

    // If queue is empty or new process has smaller value than front
    if (is_empty(queue)) {
        new_node->next = queue->front;
        queue->front = new_node;
        queue->rear = new_node;
    } else if (compare_value < ((alpha == -1) ? 
               queue->front->process->cpu_bursts[queue->front->process->index] : 
               tau_values[get_process_index(processes, n_processes, queue->front->process->id)])) {
        new_node->next = queue->front;
        queue->front = new_node;
    } else {
        // Find the correct position to insert
        Node *current = queue->front;
        while (current->next != NULL && 
               compare_value >= ((alpha == -1) ? 
               current->next->process->cpu_bursts[current->next->process->index] : 
               tau_values[get_process_index(processes, n_processes, current->next->process->id)])) {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
        if (new_node->next == NULL) queue->rear = new_node;
    }
    queue->size++;
}

void simulate_sjf(Process *processes, int n_processes, int tcs, double alpha, double lambda) {
    // Validate alpha
    if (alpha != -1 && (alpha < 0 || alpha > 1)) {
        fprintf(stderr, "ERROR: Alpha must be between 0 and 1 or -1\n");
        exit(EXIT_FAILURE);
    }

    printf("time 0ms: Simulator started for SJF [Q empty]\n");

    // Initialize tracking variables
    int current_time = 0;
    int finished_processes = 0;
    int *remaining_bursts = malloc(n_processes * sizeof(int));
    int *burst_index = calloc(n_processes, sizeof(int));
    int *io_completion_time = calloc(n_processes, sizeof(int));
    int *tau_values = malloc(n_processes * sizeof(int));
    int *wait_times = calloc(n_processes, sizeof(int));
    int *turnaround_times = calloc(n_processes, sizeof(int));
    int *last_completion_time = calloc(n_processes, sizeof(int));
    
    // Statistics variables
    int total_burst_time = 0;
    int total_context_switches = 0;
    int cb_count = 0, io_count = 0;
    float cb_wait = 0, io_wait = 0;
    float cb_turn = 0, io_turn = 0;
    int cb_context_switches = 0, io_context_switches = 0;

    // Initialize process data
    for (int i = 0; i < n_processes; i++) {
        processes[i].index = 0;
        remaining_bursts[i] = processes[i].num_bursts;
        tau_values[i] = (alpha == -1) ? 0 : (int)ceil(1.0 / lambda);
        last_completion_time[i] = processes[i].arrival_time;
    }

    Queue *ready_queue = create_queue();
    Process *cpu_process = NULL;
    int cpu_idle_until = 0, cpu_burst_end_time = -1;
    int cpu_process_index = -1;

    // Main simulation loop
    while (finished_processes < n_processes) {
        // Handle process arrivals
        for (int i = 0; i < n_processes; i++) {
            if (processes[i].arrival_time == current_time) {
                if (current_time < 10000) {
                    printf("time %dms: Process %s %s arrived; added to ready queue ",
                           current_time, processes[i].id,
                           (alpha == -1) ? "" : format_tau(tau_values[i]));
                }
                enqueue_sjf(ready_queue, &processes[i], processes, n_processes, tau_values, alpha);
                if (current_time < 10000) print_queue(ready_queue);
            }
        }

        // Handle I/O completions
        for (int i = 0; i < n_processes; i++) {
            if (io_completion_time[i] == current_time && io_completion_time[i] != 0) {
                if (current_time < 10000) {
                    printf("time %dms: Process %s %s completed I/O; added to ready queue ",
                           current_time, processes[i].id,
                           (alpha == -1) ? "" : format_tau(tau_values[i]));
                }
                last_completion_time[i] = current_time;
                enqueue_sjf(ready_queue, &processes[i], processes, n_processes, tau_values, alpha);
                if (current_time < 10000) print_queue(ready_queue);
                io_completion_time[i] = 0;
            }
        }

        // Handle CPU burst completion
        if (cpu_process != NULL && current_time == cpu_burst_end_time) {
            int actual_burst = cpu_process->cpu_bursts[burst_index[cpu_process_index]];
            total_burst_time += actual_burst;

            remaining_bursts[cpu_process_index]--;
            int bursts_left = remaining_bursts[cpu_process_index];

            if (current_time < 10000) {
                printf("time %dms: Process %s %s completed a CPU burst; %d burst%s to go ",
                    current_time, cpu_process->id,
                    (alpha == -1) ? "" : format_tau(tau_values[cpu_process_index]),
                    bursts_left,
                    bursts_left == 1 ? "" : "s");
                print_queue(ready_queue);
            }

            if (alpha != -1) {
                int old_tau = tau_values[cpu_process_index];
                tau_values[cpu_process_index] = calculate_tau2(alpha, old_tau, actual_burst);
                if (current_time < 10000) {
                    printf("time %dms: Recalculated tau for process %s: old tau %dms ==> new tau %dms ",
                        current_time, cpu_process->id, old_tau, tau_values[cpu_process_index]);
                    print_queue(ready_queue);
                }
            }

            if (bursts_left > 0) {
                int io_time = cpu_process->io_bursts[burst_index[cpu_process_index]];
                io_completion_time[cpu_process_index] = current_time + tcs/2 + io_time;
                burst_index[cpu_process_index]++;
                
                if (current_time < 10000) {
                    printf("time %dms: Process %s switching out of CPU; blocking on I/O until time %dms ",
                           current_time, cpu_process->id, io_completion_time[cpu_process_index]);
                    print_queue(ready_queue);
                }
            } else {


                printf("time %dms: Process %s terminated ", current_time, cpu_process->id);
                print_queue(ready_queue);
                
                turnaround_times[cpu_process_index] = current_time + tcs/2 - processes[cpu_process_index].arrival_time;
                if (cpu_process->is_cpu_bound) {
                    cb_count++;
                    cb_wait += wait_times[cpu_process_index];
                    cb_turn += turnaround_times[cpu_process_index];
                    cb_context_switches += burst_index[cpu_process_index] + 1;
                } else {
                    io_count++;
                    io_wait += wait_times[cpu_process_index];
                    io_turn += turnaround_times[cpu_process_index];
                    io_context_switches += burst_index[cpu_process_index] + 1;
                }
                finished_processes++;
            }

            cpu_process = NULL;
            cpu_process_index = -1;
            cpu_idle_until = current_time + tcs/2;
        }

        // Start new CPU burst if available
        if (cpu_process == NULL && !is_empty(ready_queue) && current_time >= cpu_idle_until) {
            cpu_process = dequeue(ready_queue);
            cpu_process_index = get_process_index(processes, n_processes, cpu_process->id);
            int burst_time = cpu_process->cpu_bursts[burst_index[cpu_process_index]];
            int start_time = current_time + tcs/2;

            wait_times[cpu_process_index] += (current_time - last_completion_time[cpu_process_index]);
            last_completion_time[cpu_process_index] = start_time + burst_time;

            if (current_time < 10000) {
                printf("time %dms: Process %s %s started using the CPU for %dms burst ",
                       start_time, cpu_process->id,
                       (alpha == -1) ? "" : format_tau(tau_values[cpu_process_index]),
                       burst_time);
                print_queue(ready_queue);
            }

            cpu_burst_end_time = start_time + burst_time;
            cpu_idle_until = start_time;
            total_context_switches++;
        }

        current_time++;
    }

    printf("time %dms: Simulator ended for SJF [Q empty]\n\n", current_time+1);

    FILE *f = fopen("simout.txt", "a");
    // for (int i = 0; i < n_processes; i++) {
    //     if (processes[i].is_cpu_bound) {
    //         cb_count++;
    //         cb_wait += wait_times[i];
    //         cb_turn += turnaround_times[i];
    //     } else {
    //         io_count++;
    //         io_wait += wait_times[i];
    //         io_turn += turnaround_times[i];
    //     }
    // }
    
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

    // Clean up
    free(remaining_bursts);
    free(burst_index);
    free(io_completion_time);
    free(tau_values);
    free(wait_times);
    free(turnaround_times);
    free_queue(ready_queue);
}

#endif // SIM_SJF_H