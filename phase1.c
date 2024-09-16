#include "phase1.h"
#include "process.h"

ProcessTable *table = NULL;      /* the process table */
Process *curr = NULL;      /* the current running process */
int nextID = 1;        /* next ID to assign */

void phase1_init() {
    /* TODO */
}

void TEMP_switchTo(int pid) {
    /* TODO - temporary context switch fxn */
}

int spork(char *name, int(*func)(void *), void *arg, int stacksize, int priority) {
    /* TODO - fork esque fxn
    Russ's instructions for the function:
    - Look for a slot in the process table and assign an ID
    - Fill in the details of the process
    - Set up the context structure so that we can (later) context switch to the new process
    - PART 1B ONLY: Call the dispatcher to see if it wants to do a context switch
    */
    return 0;   /* temp return - replace with return statement as spec described */
}

int join(int *status) {
    return 0;     /* temp return - replace! */
}

void quit_phase_1a(int status, int switchToPid) {
    /* TODO */
}

void quit(int status) {
    /* This is part of Phase 1b. */
}

/* 
   Returns the PID of the current process.

   Arguments: None.
   Returns: integer representing the PID of the current process.
 */
int getpid() {
    return curr->processID;
}

void dumpProcesses() {
    /* TODO */
}