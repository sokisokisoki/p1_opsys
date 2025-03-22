#include <stdlib.h>
#include <math.h>
#include <stdio.h>

typedef struct {
    char id[3]; // Process ID (e.g., "A0\0", "B1\0")
    int arrival_time; 
    int num_bursts; // Number of CPU bursts
    int *cpu_bursts; // Array of CPU burst times
    int *io_bursts; // Array of I/O burst times
} Process;

// Function to generate a processes data
void generate_process(Process *process, double lambda, double upper_bound, int is_cpu_bound) {
    // Generate arrival time
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

double next_exp(double lambda, double upper_bound) {
    while (1) {
        double r = drand48();
        double x = -log(r) / lambda;
        if (x <= upper_bound) {
            return x;
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
        processes[i].arrival_time = 0;
        processes[i].num_bursts = 0;
        processes[i].cpu_bursts = NULL;
        processes[i].io_bursts = NULL;
    }
    return processes;
}

int main(int argc, char** argv) {

    int n_processes;
    int n_cpu_processes;
    int random_seed;
    double random_lambda; 
    int random_ceiling;
    int context_switch_time;
    double alpha_sjf_srt;
    int time_slice_RR;
    handleArguments(argc, argv, &n_processes, &n_cpu_processes, &random_seed, &random_lambda, &random_ceiling, &context_switch_time, &alpha_sjf_srt, &time_slice_RR);

    //char** processes = assignProcessIDs(n_processes);
    // for (int i = 0; i < n_processes; i++) {
    //     printf("%s\n", processes[i]);
    // }
}