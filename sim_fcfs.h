void simulate_fcfs(Process *processes, int n_processes, int tcs) {
    printf("time 0ms: Simulator started for FCFS [Q empty]\n");

    int current_time = 0;
    int finished_processes = 0;

    int *remaining_bursts = (int *)malloc(n_processes * sizeof(int));
    for (int i = 0; i < n_processes; i++) {
        remaining_bursts[i] = processes[i].num_bursts;
    }

    int *burst_index = (int *)calloc(n_processes, sizeof(int));
    int *io_completion_time = (int *)calloc(n_processes, sizeof(int));
    int *wait_times = (int *)calloc(n_processes, sizeof(int));
    int *turnaround_times = (int *)calloc(n_processes, sizeof(int));
    int *last_ready_time = (int *)calloc(n_processes, sizeof(int));

    Queue *ready_queue = create_queue();

    Process *cpu_process = NULL;
    int cpu_idle_until = -1;
    int cpu_burst_end_time = -1;

    // Stats trackers
    int total_context_switches = 0;
    int total_burst_time = 0;
    int total_bursts = 0;
    int cb_context_switches = 0, io_context_switches = 0;
    int cb_count = 0, io_count = 0;

    while (finished_processes < n_processes) {
        for (int i = 0; i < n_processes; i++) {
            if (processes[i].arrival_time == current_time) {
                if (current_time < 10000)
                    printf("time %dms: Process %s arrived; added to ready queue ", current_time, processes[i].id);
                enqueue(ready_queue, &processes[i]);
                last_ready_time[i] = current_time;
                if (current_time < 10000)
                    print_queue(ready_queue);
            }
        }

        for (int i = 0; i < n_processes; i++) {
            if (io_completion_time[i] != 0 && io_completion_time[i] == current_time) {
                if (current_time < 10000)
                    printf("time %dms: Process %s completed I/O; added to ready queue ", current_time, processes[i].id);
                enqueue(ready_queue, &processes[i]);
                last_ready_time[i] = current_time;
                if (current_time < 10000)
                    print_queue(ready_queue);
                io_completion_time[i] = 0;
            }
        }

        if (cpu_process != NULL && current_time == cpu_burst_end_time) {
            int pid = cpu_process - processes;
            remaining_bursts[pid]--;
            int bursts_left = remaining_bursts[pid];

            total_burst_time += cpu_process->cpu_bursts[burst_index[pid]];
            total_bursts++;

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
            } else {
                turnaround_times[pid] = current_time - processes[pid].arrival_time;
                printf("time %dms: Process %s terminated ", current_time, cpu_process->id);
                print_queue(ready_queue);
                finished_processes++;
            }

            cpu_process = NULL;
            cpu_idle_until = current_time + tcs / 2;
        }

        if (cpu_process == NULL && !is_empty(ready_queue) && current_time >= cpu_idle_until) {
            cpu_process = dequeue(ready_queue);
            int pid = cpu_process - processes;
            int burst_time = cpu_process->cpu_bursts[burst_index[pid]];
            int start_time = current_time + tcs / 2;

            if (current_time < 10000) {
                printf("time %dms: Process %s started using the CPU for %dms burst ", start_time, cpu_process->id, burst_time);
                print_queue(ready_queue);
            }

            wait_times[pid] += (current_time - last_ready_time[pid]);
            cpu_burst_end_time = start_time + burst_time;
            cpu_idle_until = start_time;
            total_context_switches++;
            if (cpu_process->is_cpu_bound)
                cb_context_switches++;
            else
                io_context_switches++;
        }

        current_time++;
    }

    printf("time %dms: Simulator ended for FCFS [Q empty]\n\n", current_time+1);

    // Write stats
    FILE *f = fopen("simout.txt", "a");

    float cb_wait = 0, io_wait = 0, cb_turn = 0, io_turn = 0;
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

    fprintf(f, "Algorithm FCFS\n");
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
    free(wait_times);
    free(turnaround_times);
    free(last_ready_time);
    free_queue(ready_queue);
}