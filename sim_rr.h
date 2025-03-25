#ifndef SIM_RR_H
#define SIM_RR_H

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "queue.h"
#include "process.h"

void simulate_rr(Process *processes, int n_processes, int tcs, int t_slice) {
    printf("time 0ms: Simulator started for RR [Q empty]\n");

    int current_time = 0;
    int finished_processes = 0;

    int *remaining_bursts = (int *)malloc(n_processes * sizeof(int));
    int *burst_index = (int *)calloc(n_processes, sizeof(int));
    int *io_completion_time = (int *)calloc(n_processes, sizeof(int));
    int *remaining_burst_time = (int *)malloc(n_processes * sizeof(int));

    Queue *ready_queue = create_queue();

    Process *cpu_process = NULL;
    int cpu_burst_end_time = -1;
    int preempt_time = -1;
    int cpu_idle_until = -1;
    int is_context_switching = 0;

    for (int i = 0; i < n_processes; i++) {
        remaining_bursts[i] = processes[i].num_bursts;
        remaining_burst_time[i] = processes[i].cpu_bursts[0];
    }

    while (finished_processes < n_processes) {
        // arrivals
        for (int i = 0; i < n_processes; i++) {
            if (processes[i].arrival_time == current_time) {
                if (current_time < 10000) {
                    printf("time %dms: Process %s arrived; added to ready queue ", current_time, processes[i].id);
                }
                enqueue(ready_queue, &processes[i]);
                if (current_time < 10000) {
                    print_queue(ready_queue);
                }
            }
        }

        // IO completions
        for (int i = 0; i < n_processes; i++) {
            if (io_completion_time[i] != 0 && io_completion_time[i] == current_time) {
                if (current_time < 10000) {
                    printf("time %dms: Process %s completed I/O; added to ready queue ", current_time, processes[i].id);
                }
                enqueue(ready_queue, &processes[i]);
                if (current_time < 10000) {
                    print_queue(ready_queue);
                }
                io_completion_time[i] = 0;
            }
        }

        // CPU burst completion
        if (cpu_process != NULL && current_time == cpu_burst_end_time) {
            int pid = cpu_process - processes;
            int full_burst_time = processes[pid].cpu_bursts[burst_index[pid]];
            int ran_full_burst = (remaining_burst_time[pid] <= t_slice);

            if (ran_full_burst) {
                remaining_bursts[pid]--;
                int bursts_left = remaining_bursts[pid];

                if (bursts_left > 0) {
                    if (current_time < 10000) {
                        printf("time %dms: Process %s completed a CPU burst; %d bursts to go ", current_time, cpu_process->id, bursts_left);
                        print_queue(ready_queue);
                    }

                    int io_time = cpu_process->io_bursts[burst_index[pid]];
                    int io_done = current_time + tcs / 2 + io_time;
                    if (current_time < 10000) {
                        printf("time %dms: Process %s switching out of CPU; blocking on I/O until time %dms ", current_time, cpu_process->id, io_done);
                        print_queue(ready_queue);
                    }

                    io_completion_time[pid] = io_done;
                    burst_index[pid]++;
                    remaining_burst_time[pid] = processes[pid].cpu_bursts[burst_index[pid]];
                } else {
                    printf("time %dms: Process %s terminated ", current_time, cpu_process->id);
                    print_queue(ready_queue);
                    finished_processes++;
                }
            } else {
                // preempted — requeue
                if (current_time < 10000) {
                    printf("time %dms: Time slice expired; process %s preempted with %dms remaining ", current_time, cpu_process->id, remaining_burst_time[pid]);
                    print_queue(ready_queue);
                }
                enqueue(ready_queue, cpu_process);
            }

            cpu_process = NULL;
            cpu_idle_until = current_time + tcs / 2;
        }

        // start next CPU burst
        if (cpu_process == NULL && !is_empty(ready_queue) && current_time >= cpu_idle_until) {
            cpu_process = dequeue(ready_queue);
            int pid = cpu_process - processes;
            int slice = (remaining_burst_time[pid] > t_slice) ? t_slice : remaining_burst_time[pid];
            int start_time = current_time + tcs / 2;

            if (current_time < 10000) {
                printf("time %dms: Process %s started using the CPU for %dms burst ", start_time, cpu_process->id, slice);
                print_queue(ready_queue);
            }

            cpu_burst_end_time = start_time + slice;
            remaining_burst_time[pid] -= slice;
            cpu_idle_until = start_time;
        }

        current_time++;
    }

    printf("time %dms: Simulator ended for RR [Q empty]\n", current_time+1);

    free(remaining_bursts);
    free(burst_index);
    free(io_completion_time);
    free(remaining_burst_time);
    free_queue(ready_queue);
}

#endif // SIM_RR_H
