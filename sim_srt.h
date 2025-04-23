#ifndef SIM_SRT_H
#define SIM_SRT_H


#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "queue.h"
#include "process.h"


// Helper function to calculate tau (estimated burst time)
static int calculate_tau(double alpha, int previous_tau, int actual_burst) {
    return (int)ceil((alpha * actual_burst) + ((1 - alpha) * previous_tau));
}


// Comparison function for alpha-numeric sorting
int compare_process_ids(const char *id1, const char *id2) {
    // First compare the letter part
    if (id1[0] != id2[0]) {
        return id1[0] - id2[0];
    }
    // If letters are same, compare the numeric part
    return atoi(&id1[1]) - atoi(&id2[1]);
}

// Add a Process to the queue sorted by remaining time, then by process ID (for alpha = -1)
void enqueue_sorted_by_remaining_time(Queue *queue, Process *process, const int *remaining_times) {
    Node *new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) {
        fprintf(stderr, "Memory allocation failed for new node\n");
        exit(EXIT_FAILURE);
    }
    new_node->process = process;
    new_node->next = NULL;

    int new_remaining = remaining_times[process->index];

    if (is_empty(queue)) {
        queue->front = new_node;
        queue->rear = new_node;
    } 
    else {
        Node *current = queue->front;
        Node *prev = NULL;
        
        while (current != NULL) {
            int current_remaining = remaining_times[current->process->index];
            
            if (new_remaining < current_remaining) {
                break;
            }
            else if (new_remaining == current_remaining && 
                     compare_process_ids(process->id, current->process->id) < 0) {
                break;
            }
            
            prev = current;
            current = current->next;
        }
        
        if (prev == NULL) {
            new_node->next = queue->front;
            queue->front = new_node;
        } 
        else {
            new_node->next = prev->next;
            prev->next = new_node;
            
            if (new_node->next == NULL) {
                queue->rear = new_node;
            }
        }
    }
    queue->size++;
}


// Add a Process to the queue sorted by tau value, then by process ID
void enqueue_sorted_by_tau_then_id(Queue *queue, Process *process, const int *tau_values, const int *was_preempted, const int* remaining_time) {
    Node *new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) {
        fprintf(stderr, "Memory allocation failed for new node\n");
        exit(EXIT_FAILURE);
    }
    new_node->process = process;
    new_node->next = NULL;

    // Determine the comparison value for the new process
    int new_value = was_preempted[process->index] ? remaining_time[process->index] : tau_values[process->index];
    int new_tau = tau_values[process->index];

    // If queue is empty or new node should be at front
    if (is_empty(queue)) {
        queue->front = new_node;
        queue->rear = new_node;
    }
    else {
        Node *current = queue->front;
        Node *prev = NULL;
       
        // Find insertion point
        while (current != NULL) {
            // Determine comparison value for current process
            int current_value = was_preempted[current->process->index] ? 
                                remaining_time[current->process->index] : 
                               tau_values[current->process->index];
            int current_tau = tau_values[current->process->index];
           
            // First compare by the appropriate values
            if (new_value < current_value) {
                break;
            }
            // If values are equal, compare by tau
            else if (new_value == current_value) {
                if (new_tau < current_tau) {
                    break;
                }
                // If tau is also equal, compare by process ID
                else if (new_tau == current_tau &&
                         compare_process_ids(process->id, current->process->id) < 0) {
                    break;
                }
            }
           
            prev = current;
            current = current->next;
        }
       
        // Insert at front
        if (prev == NULL) {
            new_node->next = queue->front;
            queue->front = new_node;
        }
        // Insert in middle or at end
        else {
            new_node->next = prev->next;
            prev->next = new_node;
           
            // Update rear if inserting at end
            if (new_node->next == NULL) {
                queue->rear = new_node;
            }
        }
    }
    queue->size++;
}



void simulate_srt_actual(Process *processes, int n_processes, int tcs, double lambda) {
    printf("time 0ms: Simulator started for SRT [Q empty]\n");

    int current_time = 0;
    int finished_processes = 0;

    int total_cpu_busy_time = 0;
    int total_context_switches = 0;
    int cpu_bound_context_switches = 0;
    int io_bound_context_switches = 0;
    int cpu_bound_preemp = 0;
    int io_bound_preemp = 0;
    int total_preemptions = 0;

    int *total_wait_time = (int *)calloc(n_processes, sizeof(int));
    int *total_turnaround_time = (int *)calloc(n_processes, sizeof(int));

    for (int i = 0; i < n_processes; i++) {
        processes[i].index = i;
    }

    int *remaining_bursts = (int *)malloc(n_processes * sizeof(int));
    for (int i = 0; i < n_processes; i++) {
        remaining_bursts[i] = processes[i].num_bursts;
    }

    int *burst_index = (int *)calloc(n_processes, sizeof(int));
    int *io_completion_time = (int *)calloc(n_processes, sizeof(int));
    int *remaining_time = (int *)calloc(n_processes, sizeof(int));
    int *was_preempted = (int *)calloc(n_processes, sizeof(int));

    Queue *ready_queue = create_queue();
    Process *cpu_process = NULL;
    int cpu_idle_until = -1;
    int cpu_burst_end_time = -1;
    int preemption_occurred = 0;

    while (finished_processes < n_processes) {
        // Process arrivals
        for (int i = 0; i < n_processes; i++) {
            if (processes[i].arrival_time == current_time) {

                if (cpu_process != NULL) {
                    int pid = cpu_process->index;
                    int burst_time = cpu_process->cpu_bursts[burst_index[pid]];
                    int this_proc_bt = processes[i].cpu_bursts[burst_index[processes[i].index]];
                    if(this_proc_bt < burst_time){
                        if (current_time < 10000) {
                            //preempting A0 [Q A1]
                            printf("time %dms: Process %s arrived; preempting %s ", current_time, processes[i].id, cpu_process->id);
                        }
                        enqueue_sorted_by_remaining_time(ready_queue, &processes[i], remaining_time);

                        if (current_time < 10000){
                            print_queue(ready_queue);
                        }

                        was_preempted[cpu_process->index] = 1;
                        if (!processes[i].is_cpu_bound){
                            cpu_bound_preemp++;
                        }
                        else{
                            io_bound_preemp++;
                        }
                        total_preemptions++;
                    
                        remaining_time[cpu_process->index] = cpu_burst_end_time - current_time;
                        enqueue_sorted_by_remaining_time(ready_queue, cpu_process, remaining_time);
                    
                        cpu_idle_until = current_time + tcs / 2;
                        cpu_process = NULL;
                        preemption_occurred = 1;
                    }
                    else {
                        if (current_time < 10000) {
                            printf("time %dms: Process %s arrived; added to ready queue ",
                                current_time, processes[i].id);
                        }
                        remaining_time[i] = processes[i].cpu_bursts[0]; // Use actual burst time
                        enqueue_sorted_by_remaining_time(ready_queue, &processes[i], remaining_time);
                        if (current_time < 10000){
                            print_queue(ready_queue);
                        }
                    }
                }
                else {
                    if (current_time < 10000) {
                        printf("time %dms: Process %s arrived; added to ready queue ",
                            current_time, processes[i].id);
                    }
                    remaining_time[i] = processes[i].cpu_bursts[0]; // Use actual burst time
                    enqueue_sorted_by_remaining_time(ready_queue, &processes[i], remaining_time);
                    if (current_time < 10000){
                        print_queue(ready_queue);
                    }
                }
            }
        }

        // IO burst completions
        for (int i = 0; i < n_processes; i++) {
            if (io_completion_time[i] != 0 && io_completion_time[i] == current_time) {
                int pid = 0;
                int burst_time = 0;
                if (cpu_process != NULL){
                    pid = cpu_process->index;
                    burst_time = cpu_process->cpu_bursts[burst_index[pid]];
                }

                      
                int elapsed = (current_time - cpu_idle_until);   
                remaining_time[i] = processes[i].cpu_bursts[burst_index[processes[i].index]];
                remaining_time[pid] = burst_time - elapsed;
                if (cpu_process != NULL && remaining_time[i] < remaining_time[pid]) {
                    // Preemption needed
                    if (current_time < 10000) {
                        printf("time %dms: Process %s completed I/O; preempting %s ",
                              current_time, processes[i].id, cpu_process->id);
                    }
                    enqueue_sorted_by_remaining_time(ready_queue, &processes[i], remaining_time);
                    if (current_time < 10000){
                        print_queue(ready_queue);
                    }
                    
                    was_preempted[cpu_process->index] = 1;
                    if (!processes[i].is_cpu_bound){
                        cpu_bound_preemp++;
                    }
                    else{
                        io_bound_preemp++;
                    }
                    total_preemptions++;
                    
                    remaining_time[cpu_process->index] = cpu_burst_end_time - current_time;
                    enqueue_sorted_by_remaining_time(ready_queue, cpu_process, remaining_time);
                    
                    cpu_idle_until = current_time + tcs / 2;
                    cpu_process = NULL;
                    preemption_occurred = 1;
                }
                
                if (!preemption_occurred) {
                    if (current_time < 10000) {
                        printf("time %dms: Process %s completed I/O; added to ready queue ",
                              current_time, processes[i].id);
                    }
                    enqueue_sorted_by_remaining_time(ready_queue, &processes[i], remaining_time);
                    if (current_time < 10000) {
                        print_queue(ready_queue);
                    }
                }
                
                remaining_time[i] = processes[i].cpu_bursts[burst_index[i]]; // Reset with actual burst time
                io_completion_time[i] = 0;
                preemption_occurred = 0;
            }
        }

        // CPU burst completions
        if (cpu_process != NULL && current_time == cpu_burst_end_time) {
            int pid = cpu_process->index;
            int actual_burst = cpu_process->cpu_bursts[burst_index[pid]];
            
            remaining_bursts[pid]--;
            int bursts_left = remaining_bursts[pid];
            total_cpu_busy_time += actual_burst;

            if (current_time < 10000) {
                if (bursts_left > 1) {
                    printf("time %dms: Process %s completed a CPU burst; %d bursts to go ",
                      current_time, cpu_process->id, bursts_left);
                    print_queue(ready_queue);
                }
                else if (bursts_left == 0)
                {
                    /* code */
                }
                
                else {
                    printf("time %dms: Process %s completed a CPU burst; %d burst to go ",
                      current_time, cpu_process->id, bursts_left);
                    print_queue(ready_queue);
                }
            }

            if (bursts_left > 0) {
                int io_time = cpu_process->io_bursts[burst_index[pid]];
                int io_done = current_time + tcs / 2 + io_time;

                if (current_time < 10000) {
                    printf("time %dms: Process %s switching out of CPU; blocking on I/O until time %dms ",
                          current_time, cpu_process->id, io_done);
                    print_queue(ready_queue);
                }

                io_completion_time[pid] = io_done;
                burst_index[pid]++;
            } else {
                total_turnaround_time[pid] = (current_time - cpu_process->arrival_time);
                printf("time %dms: Process %s terminated ", current_time, cpu_process->id);
                print_queue(ready_queue);
                finished_processes++;
            }
            
            cpu_process = NULL;
            cpu_idle_until = current_time + tcs / 2;
        }

        // Start next CPU burst if CPU is free and ready queue is not empty
        if (cpu_process == NULL && !is_empty(ready_queue) && current_time >= cpu_idle_until) {
            cpu_process = dequeue(ready_queue);
            int pid = cpu_process->index;
            int burst_time = cpu_process->cpu_bursts[burst_index[pid]];
            int start_time = current_time + tcs / 2;
            
            if (was_preempted[pid] == 1) {
                if (current_time < 10000) {
                    printf("time %dms: Process %s started using the CPU for remaining %dms of %dms burst ",
                          current_time + tcs/2, cpu_process->id, 
                          remaining_time[pid], burst_time);
                    print_queue(ready_queue);
                }
                was_preempted[pid] = 0;
            } else {
                remaining_time[pid] = burst_time;
                if (current_time < 10000) {
                    printf("time %dms: Process %s started using the CPU for %dms burst ",
                          current_time + tcs/2, cpu_process->id, burst_time);
                    print_queue(ready_queue);
                }
            }

            if (cpu_process->is_cpu_bound){
                cpu_bound_context_switches++;
            }
            else{
                io_bound_context_switches++;
            }
            total_context_switches++;
            
            total_wait_time[pid] += (current_time - cpu_process->arrival_time);
            cpu_burst_end_time = start_time + remaining_time[pid];
            cpu_idle_until = start_time;
        }
        
        current_time++;
    }

    printf("time %dms: Simulator ended for SRT ", current_time+1);
    print_queue(ready_queue);
    printf("\n");

    double cpu_utilization = (total_cpu_busy_time * 100.0) / current_time;
    double cpu_bound_avg_wait = 0;
    double io_bound_avg_wait = 0;
    double overall_avg_wait = 0;
    double cpu_bound_avg_turnaround = 0;
    double io_bound_avg_turnaround = 0;
    double overall_avg_turnaround = 0;
    int cpu_bound_count = 0;
    int io_bound_count = 0;
    int cpu_bound_bursts = 0;
    int io_bound_bursts = 0;


    for (int i = 0; i < n_processes; i++) {
        if (processes[i].is_cpu_bound) {
            cpu_bound_avg_wait += total_wait_time[i];
            cpu_bound_avg_turnaround += total_turnaround_time[i];
            cpu_bound_count++;
            cpu_bound_bursts += processes[i].num_bursts;
        } else {
            io_bound_avg_wait += total_wait_time[i];
            io_bound_avg_turnaround += total_turnaround_time[i];
            io_bound_count++;
            io_bound_bursts += processes[i].num_bursts;
        }
    }


    int total_bursts = io_bound_bursts + cpu_bound_bursts;


    // Calculate averages (handle division by zero)
   
    cpu_bound_avg_wait = cpu_bound_count > 0 ? ceil(cpu_bound_avg_wait / total_bursts): 0;
    io_bound_avg_wait = io_bound_count > 0 ? ceil(io_bound_avg_wait / total_bursts): 0;
    overall_avg_wait = (cpu_bound_count + io_bound_count) > 0 ? ceil((cpu_bound_avg_wait + io_bound_avg_wait) / total_bursts): 0;


    cpu_bound_avg_turnaround = cpu_bound_count > 0 ? ceil(cpu_bound_avg_turnaround / total_bursts): 0;
    io_bound_avg_turnaround = io_bound_count > 0 ? ceil(io_bound_avg_turnaround / total_bursts): 0;
    overall_avg_turnaround = (cpu_bound_count + io_bound_count) > 0 ? ceil((cpu_bound_avg_turnaround + io_bound_avg_turnaround) / (cpu_bound_count + io_bound_count)): 0;


    FILE *f = fopen("simout.txt", "a");
    fprintf(f, "Algorithm SRT\n");
    fprintf(f, "-- CPU utilization: %.3f%%\n", cpu_utilization);
    fprintf(f, "-- CPU-bound average wait time: %.3f ms\n", cpu_bound_avg_wait);
    fprintf(f, "-- I/O-bound average wait time: %.3f ms\n", io_bound_avg_wait);
    fprintf(f, "-- overall average wait time: %.3f ms\n", overall_avg_wait);
    fprintf(f, "-- CPU-bound average turnaround time: %.3f ms\n", cpu_bound_avg_turnaround);
    fprintf(f, "-- I/O-bound average turnaround time: %.3f ms\n", io_bound_avg_turnaround);
    fprintf(f, "-- overall average turnaround time: %.3f ms\n", overall_avg_turnaround);
    fprintf(f, "-- CPU-bound number of context switches: %d\n", cpu_bound_context_switches);
    fprintf(f, "-- I/O-bound number of context switches: %d\n", io_bound_context_switches);
    fprintf(f, "-- overall number of context switches: %d\n", total_context_switches);
    fprintf(f, "-- CPU-bound number of preemptions: %d\n", cpu_bound_preemp);
    fprintf(f, "-- I/O-bound number of preemptions: %d\n", io_bound_preemp);
    fprintf(f, "-- overall number of preemptions: %d\n\n", total_preemptions);

    fclose(f);

    
    free(remaining_bursts);
    free(burst_index);
    free(io_completion_time);
    free(remaining_time);
    free(was_preempted);
    free_queue(ready_queue);
    free(total_turnaround_time);
    free(total_wait_time);
}




void simulate_srt(Process *processes, int n_processes, int tcs, double alpha, double lambda) {
    printf("time 0ms: Simulator started for SRT [Q empty]\n");


    int current_time = 0;
    int finished_processes = 0;


    int total_cpu_busy_time = 0;
    int total_context_switches = 0;
    int cpu_bound_context_switches = 0;
    int io_bound_context_switches = 0;
    int cpu_bound_preemp = 0;
    int io_bound_preemp = 0;
    int total_preemptions = 0;


    /*
    Calloc these arrays to track the state of each process during simulation.
    int total_wait_time;
    int total_turnaround_time;
    */
   // Track total wait time for each process
    int *total_wait_time = (int *)calloc(n_processes, sizeof(int));


    // Track total turnaround time for each process
    int *total_turnaround_time = (int *)calloc(n_processes, sizeof(int));


    // Assign an index to each process for tracking
    for (int i = 0; i < n_processes; i++) {
        processes[i].index = i;
    }


    // Track remaining CPU bursts for each process
    int *remaining_bursts = (int *)malloc(n_processes * sizeof(int));
    for (int i = 0; i < n_processes; i++) {
        remaining_bursts[i] = processes[i].num_bursts;
    }


    // Track current burst index for each process
    int *burst_index = (int *)calloc(n_processes, sizeof(int));


    // Track IO completion times
    int *io_completion_time = (int *)calloc(n_processes, sizeof(int));




    // Track tau values (estimated burst times)
    int *tau_values = (int *)malloc(n_processes * sizeof(int));
    for (int i = 0; i < n_processes; i++) {
        tau_values[i] = (int)ceil(1.0 / lambda); // Initial tau = 1/lambda
    }


    // Track remaining time for current CPU burst of each process
    int *remaining_time = (int *)calloc(n_processes, sizeof(int));


    int *predicted = (int *)calloc(n_processes, sizeof(int));


    // Track whether a process was preempted (for output formatting)
    int *was_preempted = (int *)calloc(n_processes, sizeof(int));


    // Ready queue
    Queue *ready_queue = create_queue();


    // CPU state
    Process *cpu_process = NULL;
    int cpu_idle_until = -1;
    int cpu_burst_end_time = -1;
    int preemption_occurred = 0;


    while (finished_processes < n_processes) {
        // Process arrivals
        for (int i = 0; i < n_processes; i++) {
            if (processes[i].arrival_time == current_time) {
                if (current_time < 10000) {
                    printf("time %dms: Process %s (tau %dms) arrived; added to ready queue ",
                          current_time, processes[i].id, tau_values[i]);
                }
                enqueue_sorted_by_tau_then_id(ready_queue, &processes[i], tau_values, was_preempted, remaining_time);
                remaining_time[i] = tau_values[i];
                if (current_time < 10000){
                    print_queue(ready_queue);
                }
            }
        }


        // IO burst completions
        for (int i = 0; i < n_processes; i++) {
            if (io_completion_time[i] != 0 && io_completion_time[i] == current_time) {
                int pid = 0;
                int burst_time = 0;
                if (cpu_process != NULL){
                    pid = cpu_process->index;
                    burst_time = cpu_process->cpu_bursts[burst_index[pid]];
                }
                int elapsed = (current_time - cpu_idle_until) + (burst_time - remaining_time[pid]);
                int tau = tau_values[pid];
                int p = tau - elapsed;
                predicted[pid] = p;
                if (cpu_process != NULL && p > tau_values[i]) {
                    // Preemption needed
                    if (current_time < 10000) {
                        printf("time %dms: Process %s (tau %dms) completed I/O; preempting %s (predicted remaining time %dms) ",
                              current_time, processes[i].id, tau_values[i], cpu_process->id,
                              p);
                    }
                    enqueue_sorted_by_tau_then_id(ready_queue, &processes[i], tau_values, was_preempted, predicted);
                    if (current_time < 10000){
                        print_queue(ready_queue);
                    }
                   
                    // Mark the current process as preempted
                    was_preempted[cpu_process->index] = 1;
                    if (!processes[i].is_cpu_bound){
                        cpu_bound_preemp++;
                    }
                    else{
                        io_bound_preemp++;
                    }
                    total_preemptions++;
                    // Add current process back to ready queue
                    remaining_time[cpu_process->index] = cpu_burst_end_time - current_time;
                    enqueue_sorted_by_tau_then_id(ready_queue, cpu_process, tau_values, was_preempted, predicted);
                   
                    // Start context switch to new process
                    cpu_idle_until = current_time + tcs / 2;
                    cpu_process = NULL;
                    preemption_occurred = 1;


                   
                }
               
               
                if (!preemption_occurred) {
                    if (current_time < 10000) {
                        printf("time %dms: Process %s (tau %dms) completed I/O; added to ready queue ",
                              current_time, processes[i].id, tau_values[i]);
                    }
                    enqueue_sorted_by_tau_then_id(ready_queue, &processes[i], tau_values, was_preempted, predicted);
                    if (current_time < 10000) {
                        print_queue(ready_queue);
                    }
                }
               
                remaining_time[i] = tau_values[i]; // Reset remaining time with tau
                io_completion_time[i] = 0;
                preemption_occurred = 0;
            }
        }


        // CPU burst completions
        if (cpu_process != NULL && current_time == cpu_burst_end_time) {
            int pid = cpu_process->index;
            int old_tau = tau_values[pid];
            int actual_burst = cpu_process->cpu_bursts[burst_index[pid]];
            tau_values[pid] = calculate_tau(alpha, old_tau, actual_burst);
           
            remaining_bursts[pid]--;
            int bursts_left = remaining_bursts[pid];
            total_cpu_busy_time += actual_burst;


            if (current_time < 10000) {
                if (bursts_left > 1) {
                    printf("time %dms: Process %s (tau %dms) completed a CPU burst; %d bursts to go ",
                      current_time, cpu_process->id, old_tau, bursts_left);
                    print_queue(ready_queue);
                }


                else {
                    printf("time %dms: Process %s (tau %dms) completed a CPU burst; %d burst to go ",
                      current_time, cpu_process->id, old_tau, bursts_left);
                    print_queue(ready_queue);
                }
               
                printf("time %dms: Recalculated tau for process %s: old tau %dms ==> new tau %dms ",
                      current_time, cpu_process->id, old_tau, tau_values[pid]);
                print_queue(ready_queue);
            }


            if (bursts_left > 0) {
                int io_time = cpu_process->io_bursts[burst_index[pid]];
                int io_done = current_time + tcs / 2 + io_time;


                if (current_time < 10000) {
                    printf("time %dms: Process %s switching out of CPU; blocking on I/O until time %dms ",
                          current_time, cpu_process->id, io_done);
                    print_queue(ready_queue);
                }

                predicted[pid] = 0;
                io_completion_time[pid] = io_done;
                burst_index[pid]++;
            } else {
                total_turnaround_time[pid] = (current_time - cpu_process->arrival_time);
                printf("time %dms: Process %s terminated ", current_time, cpu_process->id);
                print_queue(ready_queue);
                predicted[pid] = 0;
                finished_processes++;
            }
           
            cpu_process = NULL;
            cpu_idle_until = current_time + tcs / 2;
        }


        // Start next CPU burst if CPU is free and ready queue is not empty
        if (cpu_process == NULL && !is_empty(ready_queue) && current_time >= cpu_idle_until) {
            cpu_process = dequeue(ready_queue);
            int pid = cpu_process->index;
            int burst_time = cpu_process->cpu_bursts[burst_index[pid]];
            int start_time = current_time + tcs / 2;
           


           
            // Check if this process was preempted
            if (was_preempted[pid] == 1) {
                // This process was preempted before - use remaining time
                if (current_time < 10000) {
                    printf("time %dms: Process %s (tau %dms) started using the CPU for remaining %dms of %dms burst ",
                          current_time + tcs/2, cpu_process->id, tau_values[pid],
                          remaining_time[pid], burst_time);
                    print_queue(ready_queue);
                }
                was_preempted[pid] = 0; // Reset preemption flag
            } else {
                // Normal case - starting a fresh burst
                remaining_time[pid] = burst_time; // Set initial remaining time
                if (current_time < 10000) {
                    printf("time %dms: Process %s (tau %dms) started using the CPU for %dms burst ",
                          current_time + tcs/2, cpu_process->id, tau_values[pid], burst_time);
                    print_queue(ready_queue);
                }
            }


            if (cpu_process->is_cpu_bound){
                cpu_bound_context_switches++;
            }
            else{
                io_bound_context_switches++;
            }
            total_context_switches++;
           
            total_wait_time[pid] += (current_time - cpu_process->arrival_time);
            cpu_burst_end_time = start_time + remaining_time[pid];
            cpu_idle_until = start_time;
        }
       
        current_time++;
    }


    printf("time %dms: Simulator ended for SRT ", (current_time + tcs / 2) -1);
    print_queue(ready_queue);
    printf("\n");




    double cpu_utilization = (total_cpu_busy_time * 100.0) / current_time;
    double cpu_bound_avg_wait = 0;
    double io_bound_avg_wait = 0;
    double overall_avg_wait = 0;
    double cpu_bound_avg_turnaround = 0;
    double io_bound_avg_turnaround = 0;
    double overall_avg_turnaround = 0;
    int cpu_bound_count = 0;
    int io_bound_count = 0;
    int cpu_bound_bursts = 0;
    int io_bound_bursts = 0;


    for (int i = 0; i < n_processes; i++) {
        if (processes[i].is_cpu_bound) {
            cpu_bound_avg_wait += total_wait_time[i];
            cpu_bound_avg_turnaround += total_turnaround_time[i];
            cpu_bound_count++;
            cpu_bound_bursts += processes[i].num_bursts;
        } else {
            io_bound_avg_wait += total_wait_time[i];
            io_bound_avg_turnaround += total_turnaround_time[i];
            io_bound_count++;
            io_bound_bursts += processes[i].num_bursts;
        }
    }


    int total_bursts = io_bound_bursts + cpu_bound_bursts;


    // Calculate averages (handle division by zero)
   
    cpu_bound_avg_wait = cpu_bound_count > 0 ? ceil(cpu_bound_avg_wait / total_bursts): 0;
    io_bound_avg_wait = io_bound_count > 0 ? ceil(io_bound_avg_wait / total_bursts): 0;
    overall_avg_wait = (cpu_bound_count + io_bound_count) > 0 ? ceil((cpu_bound_avg_wait + io_bound_avg_wait) / total_bursts): 0;


    cpu_bound_avg_turnaround = cpu_bound_count > 0 ? ceil(cpu_bound_avg_turnaround / total_bursts): 0;
    io_bound_avg_turnaround = io_bound_count > 0 ? ceil(io_bound_avg_turnaround / total_bursts): 0;
    overall_avg_turnaround = (cpu_bound_count + io_bound_count) > 0 ? ceil((cpu_bound_avg_turnaround + io_bound_avg_turnaround) / (cpu_bound_count + io_bound_count)): 0;


    FILE *f = fopen("simout.txt", "a");
    fprintf(f, "Algorithm SRT\n");
    fprintf(f, "-- CPU utilization: %.3f%%\n", cpu_utilization);
    fprintf(f, "-- CPU-bound average wait time: %.3f ms\n", cpu_bound_avg_wait);
    fprintf(f, "-- I/O-bound average wait time: %.3f ms\n", io_bound_avg_wait);
    fprintf(f, "-- overall average wait time: %.3f ms\n", overall_avg_wait);
    fprintf(f, "-- CPU-bound average turnaround time: %.3f ms\n", cpu_bound_avg_turnaround);
    fprintf(f, "-- I/O-bound average turnaround time: %.3f ms\n", io_bound_avg_turnaround);
    fprintf(f, "-- overall average turnaround time: %.3f ms\n", overall_avg_turnaround);
    fprintf(f, "-- CPU-bound number of context switches: %d\n", cpu_bound_context_switches);
    fprintf(f, "-- I/O-bound number of context switches: %d\n", io_bound_context_switches);
    fprintf(f, "-- overall number of context switches: %d\n", total_context_switches);
    fprintf(f, "-- CPU-bound number of preemptions: %d\n", cpu_bound_preemp);
    fprintf(f, "-- I/O-bound number of preemptions: %d\n", io_bound_preemp);
    fprintf(f, "-- overall number of preemptions: %d\n\n", total_preemptions);

    fclose(f);


    // Cleanup
    free(remaining_bursts);
    free(burst_index);
    free(io_completion_time);
    free(tau_values);
    free(remaining_time);
    free(was_preempted);
    free_queue(ready_queue);
    free(total_turnaround_time);
    free(total_wait_time);




}


#endif // SIM_SRT_H



