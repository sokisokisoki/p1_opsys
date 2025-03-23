#ifndef PROCESS_H
#define PROCESS_H

typedef struct {
    char id[3]; // Process ID (e.g., "A0\0", "B1\0")
    int arrival_time; 
    int num_bursts; // Number of CPU bursts
    int *cpu_bursts; // Array of CPU burst times
    int *io_bursts; // Array of I/O burst times
} Process;

#endif // PROCESS_H
