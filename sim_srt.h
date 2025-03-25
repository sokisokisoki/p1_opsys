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
                enqueue(ready_queue, &processes[i]);
                remaining_time[i] = tau_values[i]; 
                if (current_time < 10000){
                    print_queue(ready_queue);
                }
            }
        }

        // IO burst completions
        for (int i = 0; i < n_processes; i++) {
            if (io_completion_time[i] != 0 && io_completion_time[i] == current_time) {
                if (cpu_process != NULL && tau_values[cpu_process->index] > tau_values[i]) {
                    // Preemption needed
                    if (current_time < 10000) {
                        printf("time %dms: Process %s (tau %dms) completed I/O; preempting %s (predicted remaining time %dms) ", 
                              current_time, processes[i].id, tau_values[i], cpu_process->id, 
                              remaining_time[cpu_process->index]);
                    }
                    enqueue(ready_queue, &processes[i]);
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
                    enqueue(ready_queue, cpu_process);
                    
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
                    enqueue(ready_queue, &processes[i]);
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
                printf("time %dms: Process %s (tau %dms) completed a CPU burst; %d bursts to go ", 
                      current_time, cpu_process->id, old_tau, bursts_left);
                print_queue(ready_queue);
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

                io_completion_time[pid] = io_done;
                burst_index[pid]++;
            } else {
                printf("time %dms: Process %s terminated ", current_time, cpu_process->id);
                print_queue(ready_queue);
                finished_processes++;
            }
            
            total_turnaround_time[pid] += current_time - cpu_idle_until;
            cpu_process = NULL;
            cpu_idle_until = current_time + tcs / 2;
        }

        // Start next CPU burst if CPU is free and ready queue is not empty
        if (cpu_process == NULL && !is_empty(ready_queue) && current_time >= cpu_idle_until) {
            cpu_process = dequeue(ready_queue);
            int pid = cpu_process->index;
            int burst_time = cpu_process->cpu_bursts[burst_index[pid]];
            


            // Check if this process was preempted
            if (was_preempted[pid]) {
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
            cpu_burst_end_time = current_time + tcs/2 + remaining_time[pid];
            cpu_idle_until = current_time + tcs/2;
        }
        
        current_time++;
    }

    printf("time %dms: Simulator ended for SRT ", current_time);
    print_queue(ready_queue);
    printf("\n");

    // // Cleanup
    // free(remaining_bursts);
    // free(burst_index);
    // free(io_completion_time);
    // free(tau_values);
    // free(remaining_time);
    // free(was_preempted);
    // free_queue(ready_queue);

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
            cpu_bound_bursts += remaining_bursts[i];
        } else {
            io_bound_avg_wait += total_wait_time[i];
            io_bound_avg_turnaround += total_turnaround_time[i];
            io_bound_count++;
            io_bound_bursts += remaining_bursts[i];
        }
    }

    // Calculate averages (handle division by zero)
    cpu_bound_avg_wait = cpu_bound_bursts > 0 ? ceil((cpu_bound_avg_wait / cpu_bound_bursts) * 1000) / 1000 : 0;
    io_bound_avg_wait = io_bound_bursts > 0 ? ceil((io_bound_avg_wait / io_bound_bursts) * 1000) / 1000 : 0;
    overall_avg_wait = (cpu_bound_bursts + io_bound_bursts) > 0 ? ceil(((cpu_bound_avg_wait * cpu_bound_bursts + io_bound_avg_wait * io_bound_bursts) / (cpu_bound_bursts + io_bound_bursts)) * 1000) / 1000 : 0;

    printf("Algorithm SRT\n");
    printf("-- CPU utilization: %.3f%%\n", cpu_utilization);
    printf("-- CPU-bound average wait time: %.3f ms\n", cpu_bound_avg_wait);
    printf("-- I/O-bound average wait time: %.3f ms\n", io_bound_avg_wait);
    printf("-- overall average wait time: %.3f ms\n", overall_avg_wait);
    printf("-- CPU-bound average turnaround time: %.3f ms\n", cpu_bound_avg_turnaround);
    printf("-- I/O-bound average turnaround time: %.3f ms\n", io_bound_avg_turnaround);
    printf("-- overall average turnaround time: %.3f ms\n", overall_avg_turnaround);
    printf("-- CPU-bound number of context switches: %d\n", cpu_bound_context_switches);
    printf("-- I/O-bound number of context switches: %d\n", io_bound_context_switches);
    printf("-- overall number of context switches: %d\n", total_context_switches);
    printf("-- CPU-bound number of preemptions: %d\n", cpu_bound_preemp);
    printf("-- I/O-bound number of preemptions: %d\n", io_bound_preemp);
    printf("-- overall number of preemptions: %d\n", total_preemptions);

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