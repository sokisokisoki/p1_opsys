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
    int *starting_burst_time = (int *)malloc(n_processes * sizeof(int));
    int *remaining_burst_time = (int *)malloc(n_processes * sizeof(int));

    Queue *ready_queue = create_queue();

    Process *cpu_process = NULL;
    int cpu_burst_end_time = -1;
    int preempt_time = -1;
    int cpu_idle_until = -1;

    // stat tracking
    int *wait_times = (int *)calloc(n_processes, sizeof(int));
    int *turnaround_times = (int *)calloc(n_processes, sizeof(int));
    int total_context_switches = 0;
    int total_preemptions = 0;
    int total_burst_time = 0;
    int total_bursts = 0;
    int cb_context_switches = 0, io_context_switches = 0;
    int cb_preemptions = 0, io_preemptions = 0;
    int cb_bursts = 0, io_bursts = 0;
    int cb_bursts_within_slice = 0, io_bursts_within_slice = 0;

    for (int i = 0; i < n_processes; i++) {
        remaining_bursts[i] = processes[i].num_bursts;
        starting_burst_time[i] = remaining_burst_time[i] = processes[i].cpu_bursts[0];
    }

    static int delay_start_time = -1;
    static int delay_pid = -1;
    static int delay_slice = -1;

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

        // io burst completions
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
        
        // cpu burst completions
        if (cpu_process != NULL && current_time == cpu_burst_end_time) {
            int pid = cpu_process - processes;
            int ran_full_burst = (remaining_burst_time[pid] == 0);
            int preemption = 1;

            if (ran_full_burst) {
                remaining_bursts[pid]--;
                int bursts_left = remaining_bursts[pid];
                total_burst_time += starting_burst_time[pid];
                total_bursts++;
                if (processes[pid].is_cpu_bound) {
                    cb_bursts++;
                    if (starting_burst_time[pid] <= t_slice) cb_bursts_within_slice++;
                } else {
                    io_bursts++;
                    if (starting_burst_time[pid] <= t_slice) io_bursts_within_slice++;
                }

                if (bursts_left > 0) {
                    if (current_time < 10000) {
                        printf("time %dms: Process %s completed a CPU burst; %d burst%sto go ", current_time, cpu_process->id, bursts_left, (bursts_left > 1 ? "s \0" : " \0"));
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
                    starting_burst_time[pid] = remaining_burst_time[pid] = processes[pid].cpu_bursts[burst_index[pid]];
                } else {
                    turnaround_times[pid] = current_time - processes[pid].arrival_time;
                    printf("time %dms: Process %s terminated ", current_time, cpu_process->id);
                    print_queue(ready_queue);
                    finished_processes++;
                }
            } else {
                if (is_empty(ready_queue)) {
                    if (current_time < 10000) {
                        printf("time %dms: Time slice expired; no preemption because ready queue is empty ", current_time);
                        print_queue(ready_queue);
                    }
                    preemption = 0;
                } else {
                    if (current_time < 10000) {
                        printf("time %dms: Time slice expired; preempting process %s with %dms remaining ", current_time, cpu_process->id, remaining_burst_time[pid]);
                        print_queue(ready_queue);
                    }
                    enqueue(ready_queue, cpu_process);
                    total_preemptions++;
                    if (cpu_process->is_cpu_bound) cb_preemptions++;
                    else io_preemptions++;
                }
            }

            if (preemption) {
                cpu_process = NULL;
                cpu_idle_until = current_time + tcs / 2;
            } else {
                int slice = (remaining_burst_time[pid] > t_slice) ? t_slice : remaining_burst_time[pid];
                int start_time = current_time;
                cpu_burst_end_time = start_time + slice;
                remaining_burst_time[pid] -= slice;
                cpu_idle_until = start_time;
            }
        }

        // starting new cpu burst
        if (cpu_process == NULL && !is_empty(ready_queue) && current_time >= cpu_idle_until && delay_start_time == -1) {
            cpu_process = dequeue(ready_queue);
            int pid = cpu_process - processes;
            int slice = (remaining_burst_time[pid] > t_slice) ? t_slice : remaining_burst_time[pid];
            delay_slice = slice;
            delay_start_time = current_time + tcs / 2;
            delay_pid = pid;
            cpu_burst_end_time = delay_start_time + slice;
            remaining_burst_time[pid] -= slice;
            cpu_idle_until = delay_start_time;

            total_context_switches++;
            if (cpu_process->is_cpu_bound) cb_context_switches++;
            else io_context_switches++;

            wait_times[pid] += (delay_start_time - processes[pid].arrival_time);
        }

        // delayed cpu burst logging
        if (cpu_process != NULL && current_time == delay_start_time) {
            int pid = delay_pid;
            if (current_time < 10000) {
                printf("time %dms: Process %s started using the CPU for ", current_time, cpu_process->id);
                if (starting_burst_time[pid] != remaining_burst_time[pid] + delay_slice) {
                    printf("remaining %dms of %dms burst ", remaining_burst_time[pid] + delay_slice, starting_burst_time[pid]);
                } else {
                    printf("%dms burst ", starting_burst_time[pid]);
                }
                print_queue(ready_queue);
            }

            delay_start_time = -1;
            delay_pid = -1;
            delay_slice = -1;
        }

        current_time++;
    }

    // end of simulation
    printf("time %dms: Simulator ended for RR [Q empty]\n\n", current_time+1);

    // output stats
    FILE *f = fopen("emp/simout.txt", "a");

    int cb_count = 0, io_count = 0;
    float cb_wait = 0, io_wait = 0, cb_turn = 0, io_turn = 0;
    int total_waits = 0;
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
        total_waits += wait_times[i];
    }

    fprintf(f, "Algorithm RR\n");
    fprintf(f, "-- CPU utilization: %.3f%%\n", (100.0 * total_burst_time) / current_time);
    fprintf(f, "-- CPU-bound average wait time: %.3f ms\n", cb_count ? cb_wait / cb_count : 0.0);
    fprintf(f, "-- I/O-bound average wait time: %.3f ms\n", io_count ? io_wait / io_count : 0.0);
    fprintf(f, "-- overall average wait time: %.3f ms\n", n_processes ? (cb_wait + io_wait) / n_processes : 0.0);
    fprintf(f, "-- CPU-bound average turnaround time: %.3f ms\n", cb_count ? cb_turn / cb_count : 0.0);
    fprintf(f, "-- I/O-bound average turnaround time: %.3f ms\n", io_count ? io_turn / io_count : 0.0);
    fprintf(f, "-- overall average turnaround time: %.3f ms\n", n_processes ? (cb_turn + io_turn) / n_processes : 0.0);
    fprintf(f, "-- CPU-bound number of context switches: %d\n", cb_context_switches);
    fprintf(f, "-- I/O-bound number of context switches: %d\n", io_context_switches);
    fprintf(f, "-- overall number of context switches: %d\n", total_context_switches);
    fprintf(f, "-- CPU-bound number of preemptions: %d\n", cb_preemptions);
    fprintf(f, "-- I/O-bound number of preemptions: %d\n", io_preemptions);
    fprintf(f, "-- overall number of preemptions: %d\n", total_preemptions);

    float cb_pct = cb_bursts ? (100.0 * cb_bursts_within_slice / cb_bursts) : 0.0;
    float io_pct = io_bursts ? (100.0 * io_bursts_within_slice / io_bursts) : 0.0;
    float all_pct = total_bursts ? (100.0 * (cb_bursts_within_slice + io_bursts_within_slice) / total_bursts) : 0.0;

    fprintf(f, "-- CPU-bound percentage of CPU bursts completed within one time slice: %.3f%%\n", cb_pct);
    fprintf(f, "-- I/O-bound percentage of CPU bursts completed within one time slice: %.3f%%\n", io_pct);
    fprintf(f, "-- overall percentage of CPU bursts completed within one time slice: %.3f%%\n", all_pct);

    fclose(f);

    // cleanup
    free(remaining_bursts);
    free(burst_index);
    free(io_completion_time);
    free(remaining_burst_time);
    free(starting_burst_time);
    free(wait_times);
    free(turnaround_times);
    free_queue(ready_queue);
}

#endif // SIM_RR_H
