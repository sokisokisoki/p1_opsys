#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "queue.h"
#include "process.h"
#include "sim_rr.h"
#include "sim_fcfs.h"
#include "sim_sjf.h"
#include "sim_srt.h"

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


void handleArguments(int argc, char *argv[], int* n_processes, int* n_cpu_processes, int* random_seed, double* random_lambda, int* random_ceiling, int* context_switch_time, double* alpha_sjf_srt, int* time_slice_RR, int* rr_alt) {
    // Check if the correct number of arguments is provided
    if (argc < 9 || argc > 10) {
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
    if (argc == 10) {
        *rr_alt = strcmp(argv[9], "RR_ALT") == 0;
    }
    

    // Validate argument constraints
    if (*n_processes <= 0 || *n_cpu_processes < 0 || *n_cpu_processes > *n_processes || *random_seed < 0 ||
        *random_lambda <= 0 || *random_ceiling <= 0 || *context_switch_time <= 0 || *context_switch_time % 2 != 0 ||
        *alpha_sjf_srt < -1 || *alpha_sjf_srt > 1 || *time_slice_RR <= 0) {
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


void print_process_conditions(int n, int n_cpu, int seed, double lambda, int bound) {
    printf("<<< -- process set (n=%d) with %d CPU-bound process%s\n", n, n_cpu, n_cpu > 1 ? "es" : "");
    printf("<<< -- seed=%d; lambda=%.6f; bound=%d\n\n", seed, lambda, bound);
}


void print_process_details(int n_processes, Process* processes) {
    for (int i = 0; i < n_processes; i++) {
        printf("%s-bound process %s: arrival time %dms; %d CPU bursts:\n", 
            (processes[i].is_cpu_bound ? "CPU" : "I/O"), processes[i].id, processes[i].arrival_time, processes[i].num_bursts);
        for (int j = 0; j < processes[i].num_bursts; j++) {
            if (j != processes[i].num_bursts - 1) {
                printf("==> CPU burst %dms ==> I/O burst %dms\n", processes[i].cpu_bursts[j], processes[i].io_bursts[j]);
            } else {
                printf("==> CPU burst %dms\n\n", processes[i].cpu_bursts[j]);
            }
        }
    }
}


void print_sim_conditions(int t_cs, double alpha, int t_slice, int rr_alt) {
    printf("<<< PROJECT SIMULATIONS\n");
    printf("<<< -- t_cs=%dms; alpha=%.2f; t_slice=%dms%s\n", t_cs, alpha, t_slice, (rr_alt ? "; RR_ALT" : ""));
}


void print_sim_stats(int n, int n_cpu) {
    FILE *f = fopen("simout.txt", "a");
    fprintf(f, "-- number of processes: %d\n", n);
    fprintf(f, "-- number of CPU-bound processes: %d\n", n_cpu);
    fprintf(f, "-- number of I/O-bound processes: %d\n", n - n_cpu);
    fprintf(f, "-- CPU-bound average CPU burst time: %.3d ms\n", 0);
    fprintf(f, "-- I/O-bound average CPU burst time: %.3d\n", 0);
    fprintf(f, "-- overall average CPU burst time: %.3d\n", 0);
    fprintf(f, "-- CPU-bound average I/O burst time: %.3d\n", 0);
    fprintf(f, "-- I/O-bound average I/O burst time: %.3d\n", 0);
    fprintf(f, "-- overall average I/O burst time: %.3d\n\n", 0);
    fclose(f);

// -- number of CPU-bound processes: 1
// -- number of I/O-bound processes: 2
// -- CPU-bound average CPU burst time: 1600.640 ms
// -- I/O-bound average CPU burst time: 247.300 ms
// -- overall average CPU burst time: 999.156 ms
// -- CPU-bound average I/O burst time: 419.084 ms
// -- I/O-bound average I/O burst time: 3277.778 ms
// -- overall average I/O burst time: 1644.239 ms
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
    int rr_alt;
    handleArguments(argc, argv, &n_processes, &n_cpu_processes, &random_seed, &random_lambda, &random_ceiling, &context_switch_time, &alpha_sjf_srt, &time_slice_RR, &rr_alt);

    // char** processIds = assignProcessIDs(n_processes);

    Process* processes = initialize_process_list(n_processes);
    assignProcessIDs(n_processes, processes);

    srand48(random_seed);

    for (int i  = 0; i <  n_processes; i++) {
        generate_process(&processes[i], random_lambda, random_ceiling, (i < n_cpu_processes ? 1 : 0));
    }

    print_process_conditions(n_processes, n_cpu_processes, random_seed, random_lambda, random_ceiling);
    print_process_details(n_processes, processes);
    print_sim_conditions(context_switch_time, alpha_sjf_srt, time_slice_RR, rr_alt);

    print_sim_stats(n_processes, n_cpu_processes);

    // simulations:
    simulate_fcfs(processes, n_processes, context_switch_time);
    simulate_sjf(processes, n_processes, context_switch_time, alpha_sjf_srt, random_lambda);
    if (alpha_sjf_srt < 0) {
        simulate_srt_actual(processes, n_processes, context_switch_time, random_lambda);
    } else {
        simulate_srt(processes, n_processes, context_switch_time, alpha_sjf_srt, random_lambda);
    }
    simulate_rr(processes, n_processes, context_switch_time, time_slice_RR, rr_alt);
}
