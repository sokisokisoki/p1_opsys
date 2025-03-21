#include <stdlib.h>
#include <math.h>

int next_exp(double lambda)
{
    double r = drand48();
    double randNum = -log(r)/lambda;

    return randNum;
}

void incorrectInput(char * binaryFile){
    printf("Inccorect Arguments Given, Expected Input: ./%s n_processes n_cpu_processes random_seed random_lambda random_ceiling context_switch_time alpha_sjf_srt time_slice_RR\n", binaryFile);
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

    
}