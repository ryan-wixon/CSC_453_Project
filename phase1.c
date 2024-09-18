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
int nextOpenSlot = 1;      /* stores next open index in process table */ // I don't think we need this because of the modulo rule
int processIDCounter = 1;  /* stores the next PID to be used */

Process *currentProcess = NULL;      /* the current running process */

char initStack[USLOSS_MIN_STACK];	/* stack for init */

void phase1_init() {
    
	// set every table entry to vacant
	for (int i = 1; i < MAXPROC; i++) {
		tableOccupancies[i] = 0;
	}

	// create the init process (will not run yet)
	Process init = { .name = "init\0", .processID = 1, .processState = 0, .priority = 6, 
			 .parent = NULL, .children = NULL, .olderSibling = NULL, .youngerSibling = NULL};
	table[1] = init;
	tableOccupancies[1] = 1;

	numProcesses++;
	nextOpenSlot++;
	processIDCounter++;

	//TODO more things
}

void TEMP_switchTo(int pid) {
    /* TODO - temporary context switch fxn
	   before doing the context switch, switch curr to the new process!
	   (do the same thing in dispatcher in phase 1b)
	 */
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

	// find the next process ID to use (0 and multiples of MAXPROC are not allowed due to the modulo rule) and create process
	while (tableOccupancies[processIDCounter % MAXPROC] != 0) {
		processIDCounter++;
	}
   	Process newProcess = { .name = name, .processID = processIDCounter, .processState = 0, .priority = priority, 
					.processMain = func, .mainArgs = arg, .parent = currentProcess, .children = NULL, 
					.olderSibling = NULL, .youngerSibling = NULL}; 
   	
	// spec says to allocate this but not sure what to do with it
	void *stack = malloc(stackSize);

	// initialize the USLOSS_Context
	USLOSS_ContextInit(newProcess->context, stack, stackSize, NULL, processWrapper);

	// add process into table
	table[processIDCounter % MAXPROC] = newProcess;
	tableOccupancies[processIDCounter % MAXPROC] = 1;

	// make new process the child of current process and sibling of
	// current process children
	Process *firstChild = currentProcess->children;
	newProcess->olderSibling = firstChild;
	firstChild->youngerSibling = newProcess;
	currentProcess->children = newProcess;
	
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
	for (int i = 1; i < MAXPROC; i++) {
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

/*
  Wrapper function for process main functions. Requires setting the new
  process that is about to be run as the current process.

  Arguments: None
  Returns: void
 */
void processWrapper() {
	/* before context switching, MUST change current process to new process!*/
	int endStatus = currentProcess->processMain(currentProcess->mainArgs);

	/* when a process's main function is over, it has quit, so call quit */
	quit_phase_1a(endStatus, currentProcess->processID);
}

/* init's "main" function (TODO: replace comment) */
void initProcessMain() {
	/* call service processes for other phases (for now these are NOPs ) */
	phase2_start_service_processes();
	phase3_start_service_processes();
	phase4_start_service_processes();
	phase5_start_service_processes();

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
