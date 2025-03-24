#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "queue.h"
#include "process.h"
  


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

void SRT(Process* processes, int n_processes, int context_switch_time, double alpha_sjf_srt, double random_lambda) {
    printf("time 0ms: Simulator started for SRT [Q empty]\n");




    int time = 0;
    int completed_processes = 0;




    Queue *ready_queue = create_queue();




    // cpu state
    Process *cpu_process = NULL;
    int cpu_idle_until = -1;
    //int is_context_switching = 0;
    int cpu_burst_end_time = -1;




    // track IO completions
    int *io_completion_time = (int *)calloc(n_processes, sizeof(int));
    int *remaining_bursts = (int *)malloc(n_processes * sizeof(int));
    int *remaining_burst_time = (int *)malloc(n_processes * sizeof(int));
    int *tau = (int *)malloc(n_processes * sizeof(int));
    int *burst_index = (int *)calloc(n_processes, sizeof(int));




   




    // Initialize remaining_bursts, tau, and remaining burst times
    for (int i = 0; i < n_processes; i++) {
        remaining_bursts[i] = processes[i].num_bursts;
        //remaining_burst_time[i] = processes[i].cpu_bursts[0];
        tau[i] = (int)ceil(1.0 / random_lambda);
    }




    while (completed_processes < n_processes) {
        //Process Arrivals
        for (int i = 0; i < n_processes; i++) {
            if (processes[i].arrival_time == time) {
                if (time < 10000) {
                    printf("time %dms: Process %s (tau %dms) arrived; added to ready queue ", time, processes[i].id, tau[i]);
                }
                enqueue(ready_queue, &processes[i]);
                if (time < 10000) {
                    print_queue(ready_queue);
                }
            }
        }


       


       // IO burst completions
       for (int i = 0; i < n_processes; i++) {
        if (io_completion_time[i] != 0 && io_completion_time[i] == time) {
            // Process completed I/O and is now ready
            if (time < 10000) {
                printf("time %dms: Process %s (tau %dms) completed I/O",
                       time, processes[i].id, tau[i]);
            }
   
            // Check if we should preempt the current process
            if (cpu_process != NULL) {
                int current_pid = cpu_process - processes;
                int remaining_current = cpu_burst_end_time - time;
               
                // Preempt if new process has shorter remaining time (using tau)
                if (tau[i] < tau[current_pid]) {
                    if (time < 10000) {
                        printf("; preempting %s (predicted remaining time %dms) ",
                               cpu_process->id, remaining_current);
                    }
                   
                    // Save remaining time for preempted process
                    remaining_burst_time[current_pid] = remaining_current;
                   
                    // Move preempted process back to ready queue
                    enqueue(ready_queue, &processes[i]);
                    if (time < 10000) {
                        print_queue(ready_queue);
                    }
                    enqueue(ready_queue, cpu_process);
                   
                    // Schedule context switch to new process
                    cpu_process = NULL;
                    // int burst_time = processes[i].cpu_bursts[burst_index[i]];
                    //cpu_burst_end_time = time + (context_switch_time / 2) + burst_time;
                    cpu_idle_until = time + (context_switch_time / 2);
                   


                   
                    io_completion_time[i] = 0;
                    continue; // Skip the normal enqueue below
                }
            }
   
            // If no preemption, just add to ready queue
            enqueue(ready_queue, &processes[i]);
            if (time < 10000) {
                printf("; added to ready queue ");
                print_queue(ready_queue);
            }
            io_completion_time[i] = 0;
        }
    }






        // cpu burst completions
        if (cpu_process != NULL && time == cpu_burst_end_time) {
            int pid = cpu_process - processes;
            remaining_bursts[pid]--;
            int bursts_left = remaining_bursts[pid];
            int burst_time = cpu_process->cpu_bursts[burst_index[pid]];


            if (bursts_left > 0) {
                if (time < 10000) {
                    printf("time %dms: Process %s (tau %dms) completed a CPU burst; %d bursts to go ", time, cpu_process->id, tau[pid], bursts_left);
                    print_queue(ready_queue);
                    int old_tau = tau[pid];
                    tau[pid] = (int)ceil(alpha_sjf_srt * burst_time)  + ((1 - alpha_sjf_srt) * old_tau);
                    printf("time %dms: Recalculated tau for process %s: old tau %dms ==> new tau %dms ", time, cpu_process->id, old_tau, tau[pid]);
                    print_queue(ready_queue);
                }


                int io_time = cpu_process->io_bursts[burst_index[pid]];
                int io_done = time + context_switch_time / 2 + io_time;


                if (time < 10000) {
                    printf("time %dms: Process %s switching out of CPU; blocking on I/O until time %dms ", time, cpu_process->id, io_done);
                    print_queue(ready_queue);
                }


                io_completion_time[pid] = io_done;
                burst_index[pid]++;
            } else {
                printf("time %dms: Process %s terminated ", time, cpu_process->id);
                print_queue(ready_queue);
                completed_processes++;
            }


            cpu_process = NULL;
            //is_context_switching = 1;
            cpu_idle_until = time + context_switch_time / 2;
        }


        // start next cpu burst
        if (cpu_process == NULL && !is_empty(ready_queue) && time >= cpu_idle_until) {
            cpu_process = dequeue(ready_queue);
            int pid = cpu_process - processes;
           
            // Determine the actual burst time (full burst or remaining from preemption)
            int burst_time;
            if (remaining_burst_time[pid] > 0) {
                // This process was preempted - use remaining time
                burst_time = remaining_burst_time[pid];
                remaining_burst_time[pid] = 0; // Reset for next time
            } else {
                // New burst - use full time from bursts array
                burst_time = cpu_process->cpu_bursts[burst_index[pid]];
            }
           
            int start_time = time + (context_switch_time / 2);
           
            if (time < 10000) {
                printf("time %dms: Process %s (tau %dms) ", start_time, cpu_process->id, tau[pid]);
                if (remaining_burst_time[pid] > 0) {
                    printf("started using the CPU for remaining %dms of %dms burst ",
                           burst_time,
                           cpu_process->cpu_bursts[burst_index[pid]]);
                } else {
                    printf("started using the CPU for %dms burst ", burst_time);
                }
                print_queue(ready_queue);
            }
       
            cpu_burst_end_time = start_time + burst_time;
            cpu_idle_until = start_time;
        }






        time++;
    }




    printf("time %dms: Simulator ended for SRT [Q empty]\n", time+1);




    free(remaining_bursts);
    free(remaining_burst_time);
    free(tau);
    free(io_completion_time);
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


void print_process_conditions(int n, int n_cpu, int seed, double lambda, int bound) {
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

void print_sim_conditions(int t_cs, double alpha, int t_slice) {
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

    print_process_conditions(n_processes, n_cpu_processes, random_seed, random_lambda, random_ceiling);
    print_process_details(n_processes, processes);
    print_sim_conditions(context_switch_time, alpha_sjf_srt, time_slice_RR);


    SRT(processes, n_processes, context_switch_time, alpha_sjf_srt, random_lambda);
    
}
