#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "queue.h"
#include "process.h"

// typedef struct {
//     char id[3]; // Process ID (e.g., "A0\0", "B1\0")
//     int arrival_time; 
//     int num_bursts; // Number of CPU bursts
//     int *cpu_bursts; // Array of CPU burst times
//     int *io_bursts; // Array of I/O burst times
// } Process;


void print_queue(Queue *q) {
    printf("[Q");
    if (is_empty(q)) {
        printf(" empty");
    } else {
        Node *curr = q->front;
        while (curr != NULL) {
            printf(" %s", curr->process->id);
            curr = curr->next;
        }
    }
    printf("]\n");
}


void simulate_fcfs(Process *processes, int n_processes, int tcs) {
    printf("time 0ms: Simulator started for FCFS [Q empty]\n");

    int current_time = 0;
    int finished_processes = 0;

    // Track remaining CPU bursts
    int *remaining_bursts = (int *)malloc(n_processes * sizeof(int));
    for (int i = 0; i < n_processes; i++) {
        remaining_bursts[i] = processes[i].num_bursts;
    }

    // Track burst index per process
    int *burst_index = (int *)calloc(n_processes, sizeof(int));

    // Track I/O completions
    int *io_completion_time = (int *)calloc(n_processes, sizeof(int));

    // Ready queue
    Queue *ready_queue = create_queue();

    // CPU state
    Process *cpu_process = NULL;
    int cpu_idle_until = -1;
    int is_context_switching = 0;
    int cpu_burst_end_time = -1;

    while (finished_processes < n_processes) {
        // ---------------- 1. Arrivals ----------------
        for (int i = 0; i < n_processes; i++) {
            if (processes[i].arrival_time == current_time) {
                printf("time %dms: Process %s arrived; added to ready queue ", current_time, processes[i].id);
                enqueue(ready_queue, &processes[i]);
                print_queue(ready_queue);
            }
        }

        // ---------------- 2. I/O completions ----------------
        for (int i = 0; i < n_processes; i++) {
            if (io_completion_time[i] == current_time) {
                printf("time %dms: Process %s completed I/O; added to ready queue ", current_time, processes[i].id);
                enqueue(ready_queue, &processes[i]);
                print_queue(ready_queue);
                io_completion_time[i] = 0;
            }
        }

        // ---------------- 3. Check for CPU burst completion ----------------
        if (cpu_process != NULL && current_time == cpu_burst_end_time) {
            int pid = cpu_process - processes;
            remaining_bursts[pid]--;
            int bursts_left = remaining_bursts[pid];

            if (bursts_left > 0) {
                printf("time %dms: Process %s completed a CPU burst; %d bursts to go ", current_time, cpu_process->id, bursts_left);
                print_queue(ready_queue);

                int io_time = cpu_process->io_bursts[burst_index[pid]];
                int io_done = current_time + tcs / 2 + io_time;

                printf("time %dms: Process %s switching out of CPU; blocking on I/O until time %dms ", current_time, cpu_process->id, io_done);
                print_queue(ready_queue);

                io_completion_time[pid] = io_done;
                burst_index[pid]++;
            } else {
                printf("time %dms: Process %s terminated ", current_time, cpu_process->id);
                print_queue(ready_queue);
                finished_processes++;
            }

            cpu_process = NULL;
            is_context_switching = 1;
            cpu_idle_until = current_time + tcs / 2;
        }

        // ---------------- 4. CPU is idle and ready to pick next ----------------
        if (cpu_process == NULL && !is_empty(ready_queue) && current_time >= cpu_idle_until) {
            cpu_process = dequeue(ready_queue);
            int pid = cpu_process - processes;
            int burst_time = cpu_process->cpu_bursts[burst_index[pid]];
            int start_time = current_time + tcs / 2;

            printf("time %dms: Process %s started using the CPU for %dms burst ", start_time, cpu_process->id, burst_time);
            print_queue(ready_queue);

            cpu_burst_end_time = start_time + burst_time;
            cpu_idle_until = start_time; // CPU won't be idle once switch-in ends
            is_context_switching = 1;
        }

        current_time++;
    }

    printf("time %dms: Simulator ended for FCFS [Q empty]\n", current_time);

    // Cleanup
    free(remaining_bursts);
    free(burst_index);
    free(io_completion_time);
    free_queue(ready_queue);
}


double next_exp(double lambda, double upper_bound) {
    while (1) {
        double r = drand48();
        double x = -log(r) / lambda;
        if (x <= upper_bound) {
            return x;
        }
    }
}


// Function to generate a processes data
void generate_process(Process *process, double lambda, double upper_bound, int is_cpu_bound) {
    // Generate arrival time
    process->is_cpu_bound = is_cpu_bound;
    process->arrival_time = (int)floor(next_exp(lambda, upper_bound));

    // Generate number of CPU bursts (1 to 32)
    process->num_bursts = (int)ceil(drand48() * 32);

    // Allocate memory for CPU and I/O bursts
    process->cpu_bursts = (int *)malloc(process->num_bursts * sizeof(int));
    process->io_bursts = (int *)malloc((process->num_bursts - 1) * sizeof(int));

    // Generate CPU and I/O burst times
    for (int i = 0; i < process->num_bursts; i++) {
        process->cpu_bursts[i] = (int)ceil(next_exp(lambda, upper_bound)) * (is_cpu_bound ? 4 : 1);
        if (i < process->num_bursts - 1) {
            process->io_bursts[i] = (int)ceil(next_exp(lambda, upper_bound)) * 8 / (is_cpu_bound ? 8: 1);
        }
    }
}


void incorrectInput(char * binaryFile){
    fprintf(stderr, "Inccorect Arguments Given, Expected Input: ./%s n_processes n_cpu_processes random_seed random_lambda random_ceiling context_switch_time alpha_sjf_srt time_slice_RR\n", binaryFile);
    exit(EXIT_FAILURE);
}


void handleArguments(int argc, char *argv[], int* n_processes, int* n_cpu_processes, int* random_seed, double* random_lambda, int* random_ceiling, int* context_switch_time, double* alpha_sjf_srt, int* time_slice_RR) {
    // Check if the correct number of arguments is provided
    if (argc != 9) {
        incorrectInput(argv[0]);
    }

    // Parse and validate command-line arguments
    *n_processes = atoi(argv[1]);
    *n_cpu_processes = atoi(argv[2]);
    *random_seed = atoi(argv[3]);
    *random_lambda = atof(argv[4]);
    *random_ceiling = atoi(argv[5]);
    *context_switch_time = atoi(argv[6]);
    *alpha_sjf_srt = atof(argv[7]);
    *time_slice_RR = atoi(argv[8]);

    // Validate argument constraints
    if (*n_processes <= 0 || *n_cpu_processes < 0 || *n_cpu_processes > *n_processes || *random_seed < 0 ||
        *random_lambda <= 0 || *random_ceiling <= 0 || *context_switch_time <= 0 || *context_switch_time % 2 != 0 ||
        *alpha_sjf_srt < 0 || *alpha_sjf_srt > 1 || *time_slice_RR <= 0) {
        incorrectInput(argv[0]);
    }
}


void assignProcessIDs(int n_processes, Process *processes) {
    char letter = 'A';
    int num = 0;

    for (int i = 0; i < n_processes; i++) {
        // Assign the ID directly to the Process struct
        snprintf(processes[i].id, sizeof(processes[i].id), "%c%c", letter, (char)(num + '0'));
        num++;
        if (num == 10) {
            letter++;
            num = 0;
        }
    }
}


//Function to initialize a list of empty Process structs
Process* initialize_process_list(int n_processes) {
    Process* processes = (Process*)malloc(n_processes * sizeof(Process));
    for (int i = 0; i < n_processes; i++) {
        // Initialize each Process struct with default values
        strcpy(processes[i].id, ""); // Empty ID
        processes[i].is_cpu_bound = 0;
        processes[i].arrival_time = 0;
        processes[i].num_bursts = 0;
        processes[i].cpu_bursts = NULL;
        processes[i].io_bursts = NULL;
    }
    return processes;
}


void print_process_gen_details(int n, int n_cpu, int seed, double lambda, int bound) {
    printf("<<< -- process set (n=%d) with %d CPU-bound process\n", n, n_cpu);
    printf("<<< -- seed=%d; lambda=%.6f; bound=%d\n\n", seed, lambda, bound);
}


void print_process_details(int n_processes, Process* processes) {
    for (int i = 0; i < n_processes; i++) {
        printf("%s-bound process %s: arrival time %dms; %d CPU bursts:\n", 
            (processes[i].is_cpu_bound ? "CPU" : "IO"), processes[i].id, processes[i].arrival_time, processes[i].num_bursts);
        for (int j = 0; j < processes[i].num_bursts; j++) {
            if (j != processes[i].num_bursts - 1) {
                printf("==> CPU burst %dms ==> I/O burst %dms\n", processes[i].cpu_bursts[j], processes[i].io_bursts[j]);
            } else {
                printf("==> CPU burst %dms\n\n", processes[i].cpu_bursts[j]);
            }
        }
    }
}

void print_sim_gen_details(int t_cs, double alpha, int t_slice) {
    printf("<<< PROJECT SIMULATIONS\n");
    printf("<<< -- t_cs=%dms; alpha=%.2f; t_slice=%dms\n", t_cs, alpha, t_slice);
}

int main(int argc, char** argv) {
    setvbuf( stdout, NULL, _IONBF, 0 );
    int n_processes;
    int n_cpu_processes;
    int random_seed;
    double random_lambda; 
    int random_ceiling;
    int context_switch_time;
    double alpha_sjf_srt;
    int time_slice_RR;
    handleArguments(argc, argv, &n_processes, &n_cpu_processes, &random_seed, &random_lambda, &random_ceiling, &context_switch_time, &alpha_sjf_srt, &time_slice_RR);

    // char** processIds = assignProcessIDs(n_processes);

    Process* processes = initialize_process_list(n_processes);
    assignProcessIDs(n_processes, processes);

    srand48(random_seed);

    for (int i  = 0; i <  n_processes; i++) {
        generate_process(&processes[i], random_lambda, random_ceiling, (i < n_cpu_processes ? 1 : 0));
    }

    print_process_gen_details(n_processes, n_cpu_processes, random_seed, random_lambda, random_ceiling);
    print_process_details(n_processes, processes);
    print_sim_gen_details(context_switch_time, alpha_sjf_srt, time_slice_RR);

    simulate_fcfs(processes, n_processes, context_switch_time);
    
    // for (int i = 0; i < n_processes; i++) {
    //     printf("%s\n", processes[i]);
    // }
}