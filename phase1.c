#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "phase1.h"
#include "process.h"

/* the process table */
Process table[MAXPROC];     
int tableOccupancies[MAXPROC];

int numProcesses = 0;	   /* stores number of currently running processes */
int nextOpenSlot = 0;      /* stores next open index in process table */ // I don't think we need this because of the modulo rule
int processIDCounter = 0;  /* stores the next PID to be used */

Process *currentProcess = NULL;      /* the current running process */

char initStack[USLOSS_MIN_STACK];	/* stack for init */

void phase1_init() {
    
	// set every table entry to vacant
	for (int i = 0; i < MAXPROC; i++) {
		tableOccupancies[i] = 0;
	}

	// create the init process (will not run yet)
	Process init = { .name = "init\0", .processID = 1, .processState = 0, .priority = 6, 
			 .parent = NULL, .children = NULL, .olderSibling = NULL, .youngerSibling = NULL};
	table[0] = init;
	tableOccupancies[0] = 1;

	numProcesses++;
	nextOpenSlot++;
	processIDCounter++;

	//TODO more things
}

void TEMP_switchTo(int pid) {
    /* TODO - temporary context switch fxn */
}

int spork(char *name, int(*func)(void *), void *arg, int stackSize, int priority) {
    /* TODO - fork esque fxn
    Russ's instructions for the function:
    - Look for a slot in the process table and assign an ID (numProcesses + 1)
    - Fill in the details of the process
    - Set up the context structure so that we can (later) context switch to the new process
    - PART 1B ONLY: Call the dispatcher to see if it wants to do a context switch
    */

	// check for errors; return -2 if stack size is too small and -1 for all else
	if (stackSize < USLOSS_MIN_STACK) {
		return -2;
	}
	if (numProcesses == MAXPROC || priority < 0 || priority > 7 || func == NULL || name == NULL || strlen(name) > pow(MAXNAME, 7)) {
		return -1;
	}

	// find the next process ID to use and create process
	while (tableOccupancies[processIDCounter % MAXPROC] != 0) {
		processIDCounter++;
	}
   	Process newProcess = { .name = name, .processID = processIDCounter, .processState = 0, .priority = priority, 
			       .parent = currentProcess, .children = NULL, .olderSibling = NULL, .youngerSibling = NULL}; 
   	
	// add process into table
	table[processIDCounter % MAXPROC] = newProcess;
	tableOccupancies[processIDCounter % MAXPROC] = 1;

	// spec says to allocate this but not sure what to do with it
	void *stack = malloc(stackSize);

	// TODO: USLOSS_ContextInit
	
	return newProcess.processID;
}

int join(int *status) {
    return 0;     /* temp return - replace! */
}

void quit_phase_1a(int status, int switchToPid) {
    /* TODO */
}

void quit(int status) {
    /* This is part of Phase 1b and is NOP for now. */
}

/* 
   Returns the PID of the current process.

   Arguments: None.
   Returns: integer representing the PID of the current process.
 */
int getpid() {
    return currentProcess->processID;
}

/*
 * Prints the current process table.
 *
 * Arguments: None
 *   Returns: Void 
 */
void dumpProcesses() {
	printf("Process Dump\n------------------------------\n");
	for (int i = 0; i < MAXPROC; i++) {
		printf("  Table Index: %d\n\n", i);
		if (tableOccupancies[i] == 1) {
			printProcess(&table[i]);
		}
		else {
			printf("  EMPTY\n");
		}
		printf("\n------------------------------\n");    
	}
}

/* init's "main" function */
void initProcessMain() {
	/* call service processes for other phases (for now these are NOPs ) */
	
	// phase2_start_service_processes();
	// phase3_start_service_processes();
	// phase4_start_service_processes();
	// phase5_start_service_processes();

	/* TODO
	   create testcase_main (need to use spork once it's done)
	   enter join loop (need completed join)
	   if join returns an error, terminate the program
	*/
}


// delete this later, using to test spork 
int dummy() {
	return 0;
}

// nothing in here is final, just using for testing
int main() {

	phase1_init();
	spork("sporkTest", &dummy, NULL, USLOSS_MIN_STACK, 3);

	dumpProcesses();

	return 0;
}
