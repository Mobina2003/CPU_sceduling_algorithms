#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For memcpy
#include <limits.h> // For INT_MAX

// Helper function to sort processes by arrival time
static int compareArrival(const void *a, const void *b) {
    return ((Process *)a)->arrivalTime - ((Process *)b)->arrivalTime;
}

// Helper function for SJF (not used directly in current SJF logic, but good to have)
static int compareBurst(const void *a, const void *b) {
    return ((Process *)a)->burstTime - ((Process *)b)->burstTime;
}

// FCFS Scheduling
Metrics fcfs_metrics(Process proc[], int n) {
    // Create a working copy of processes to avoid modifying original array
    Process *processes = (Process *)malloc(n * sizeof(Process));
    if (processes == NULL) {
        fprintf(stderr, "Memory allocation failed for FCFS processes.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(processes, proc, n * sizeof(Process));

    // Sort processes by arrival time
    qsort(processes, n, sizeof(Process), compareArrival);

    float totalTurnaroundTime = 0;
    float totalWaitingTime = 0;
    float totalResponseTime = 0;
    int currentTime = 0;

    for (int i = 0; i < n; i++) {
        // If current time is before process arrival, advance time to process arrival
        if (currentTime < processes[i].arrivalTime) {
            currentTime = processes[i].arrivalTime;
        }

        processes[i].startTime = currentTime;
        processes[i].completionTime = currentTime + processes[i].burstTime;

        int turnaroundTime = processes[i].completionTime - processes[i].arrivalTime;
        int waitingTime = processes[i].startTime - processes[i].arrivalTime;
        int responseTime = processes[i].startTime - processes[i].arrivalTime;

        totalTurnaroundTime += turnaroundTime;
        totalWaitingTime += waitingTime;
        totalResponseTime += responseTime;

        currentTime = processes[i].completionTime;
    }

    Metrics metrics;
    metrics.avgTurnaround = totalTurnaroundTime / n;
    metrics.avgWaiting = totalWaitingTime / n;
    metrics.avgResponse = totalResponseTime / n;

    free(processes);
    return metrics;
}

// SJF Scheduling (Non-preemptive)
Metrics sjf_metrics(Process proc[], int n) {
    // Create a working copy of processes
    Process *processes = (Process *)malloc(n * sizeof(Process));
    if (processes == NULL) {
        fprintf(stderr, "Memory allocation failed for SJF processes.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(processes, proc, n * sizeof(Process));

    for (int i = 0; i < n; i++) {
        processes[i].startTime = -1; // Indicate not started
    }

    float totalTurnaroundTime = 0;
    float totalWaitingTime = 0;
    float totalResponseTime = 0;
    int currentTime = 0;
    int completedProcesses = 0;

    int *isCompleted = (int *)calloc(n, sizeof(int));
    if (isCompleted == NULL) {
        fprintf(stderr, "Memory allocation failed for SJF isCompleted array.\n");
        free(processes);
        exit(EXIT_FAILURE);
    }

    while (completedProcesses < n) {
        int shortestJobIndex = -1;
        int minBurst = INT_MAX;

        // Find the shortest job that has arrived and is not completed
        for (int i = 0; i < n; i++) {
            if (processes[i].arrivalTime <= currentTime && isCompleted[i] == 0) {
                if (processes[i].burstTime < minBurst) {
                    minBurst = processes[i].burstTime;
                    shortestJobIndex = i;
                }
            }
        }

        if (shortestJobIndex == -1) {
            // CPU is idle, advance time to the next arrival if any
            int nextArrivalTime = INT_MAX;
            for (int i = 0; i < n; i++) {
                if (isCompleted[i] == 0 && processes[i].arrivalTime < nextArrivalTime) {
                    nextArrivalTime = processes[i].arrivalTime;
                }
            }
            if (nextArrivalTime != INT_MAX && currentTime < nextArrivalTime) {
                currentTime = nextArrivalTime;
            } else {
                currentTime++; // Just advance time if no future arrivals or something unexpected
            }
        } else {
            Process *p = &processes[shortestJobIndex];

            if (p->startTime == -1) {
                p->startTime = currentTime;
            }

            p->completionTime = currentTime + p->burstTime;
            currentTime = p->completionTime;

            int turnaroundTime = p->completionTime - p->arrivalTime;
            int waitingTime = p->startTime - p->arrivalTime;
            int responseTime = p->startTime - p->arrivalTime;

            totalTurnaroundTime += turnaroundTime;
            totalWaitingTime += waitingTime;
            totalResponseTime += responseTime;

            isCompleted[shortestJobIndex] = 1;
            completedProcesses++;
        }
    }

    Metrics metrics;
    metrics.avgTurnaround = totalTurnaroundTime / n;
    metrics.avgWaiting = totalWaitingTime / n;
    metrics.avgResponse = totalResponseTime / n;

    free(processes);
    free(isCompleted);
    return metrics;
}


// Round Robin Scheduling
Metrics rr_metrics(Process proc[], int n, int timeQuantum) {
    Process *processes = (Process *)malloc(n * sizeof(Process));
    if (processes == NULL) {
        fprintf(stderr, "Memory allocation failed for RR processes.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(processes, proc, n * sizeof(Process));

    for (int i = 0; i < n; i++) {
        processes[i].remainingTime = processes[i].burstTime;
        processes[i].startTime = -1; // Sentinel value
    }

    float totalTurnaroundTime = 0;
    float totalWaitingTime = 0;
    float totalResponseTime = 0;
    int currentTime = 0;
    int completedProcesses = 0;

    // Simple circular queue using an array
    int *queue = (int *)malloc(n * sizeof(int)); 
    if (queue == NULL) {
        fprintf(stderr, "Memory allocation failed for RR queue.\n");
        free(processes);
        exit(EXIT_FAILURE);
    }
    int front = 0, rear = -1, queueSize = 0;

    // To track if a process (by its original index) is currently in the queue
    int *inQueue = (int *)calloc(n, sizeof(int)); 
    if (inQueue == NULL) {
        fprintf(stderr, "Memory allocation failed for RR inQueue array.\n");
        free(processes);
        free(queue);
        exit(EXIT_FAILURE);
    }

    while (completedProcesses < n) {
        // Step 1: Add newly arrived processes to the queue
        // Processes must be added in arrival order to maintain fairness among concurrent arrivals.
        // We'll iterate through original process list's order.
        // It's important to do this here, *before* processing from the queue,
        // to correctly handle processes that arrive simultaneously or just before
        // an existing process finishes its quantum.
        for (int i = 0; i < n; i++) {
            if (processes[i].arrivalTime <= currentTime && processes[i].remainingTime > 0 && inQueue[i] == 0) {
                rear = (rear + 1) % n;
                queue[rear] = i; // Store original index
                inQueue[i] = 1;
                queueSize++;
            }
        }

        if (queueSize == 0) {
            // CPU is idle. Find the earliest arrival time of an uncompleted process.
            int nextArrivalTime = INT_MAX;
            for(int i = 0; i < n; i++) {
                if(processes[i].remainingTime > 0 && processes[i].arrivalTime < nextArrivalTime) {
                    nextArrivalTime = processes[i].arrivalTime;
                }
            }
            if (nextArrivalTime == INT_MAX) { // No more processes to arrive/complete, we are done
                break; 
            }
            // Advance time to the next process arrival
            if (currentTime < nextArrivalTime) {
                currentTime = nextArrivalTime;
            }
            // If currentTime is already at or past nextArrivalTime, it means processes
            // should have been added in the previous loop iteration.
            continue; // Re-check queue and arrivals at the new currentTime
        }

        // Step 2: Dequeue and execute a process
        int processIndex = queue[front]; 
        front = (front + 1) % n; // Dequeue
        queueSize--;
        inQueue[processIndex] = 0; // Mark as not in queue temporarily

        Process *p = &processes[processIndex];

        if (p->startTime == -1) { // Set start time only the first time the process runs
            p->startTime = currentTime;
        }

        int executionTime = (p->remainingTime < timeQuantum) ? p->remainingTime : timeQuantum;
        
        p->remainingTime -= executionTime;
        currentTime += executionTime;

        // Step 3: Add new arrivals that occurred *during* this execution slice
        // This loop is crucial for correct Round Robin simulation.
        for (int i = 0; i < n; i++) {
            // Check only processes that were not already in queue and are not the currently running process
            if (i != processIndex && processes[i].arrivalTime <= currentTime && 
                processes[i].remainingTime > 0 && inQueue[i] == 0) {
                rear = (rear + 1) % n;
                queue[rear] = i;
                inQueue[i] = 1;
                queueSize++;
            }
        }

        // Step 4: Handle completion or re-queuing
        if (p->remainingTime == 0) { // Process completed
            p->completionTime = currentTime;
            int turnaroundTime = p->completionTime - p->arrivalTime;
            int waitingTime = turnaroundTime - p->burstTime;
            int responseTime = p->startTime - p->arrivalTime;

            totalTurnaroundTime += turnaroundTime;
            totalWaitingTime += waitingTime;
            totalResponseTime += responseTime;
            completedProcesses++;
        } else { // Process not completed, add back to queue
            rear = (rear + 1) % n;
            queue[rear] = processIndex;
            inQueue[processIndex] = 1; // Mark back in queue
            queueSize++;
        }
    }

    Metrics metrics;
    metrics.avgTurnaround = totalTurnaroundTime / n;
    metrics.avgWaiting = totalWaitingTime / n;
    metrics.avgResponse = totalResponseTime / n;

    free(processes);
    free(queue);
    free(inQueue);
    return metrics;
}