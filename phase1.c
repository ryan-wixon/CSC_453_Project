#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "phase1.h"
#include "process.h"

Process table[MAXPROC];     	/* the process table */
int tableOccupancies[MAXPROC]; 	/* keeps track of which process table slots are empty and which are full */

int numProcesses = 1;	   	/* stores number of currently existing processes; starts at 1 because of init */
int processIDCounter = 2;  	/* stores the next PID to be used; starts at 2 because init is PID 1 */

Process *currentProcess = NULL;      	/* the current running process */

char initStack[USLOSS_MIN_STACK];	/* stack for init */

void phase1_init() {
    
	// set every table entry to vacant
	for (int i = 0; i < MAXPROC; i++) {
		tableOccupancies[i] = 0;
	}

	// create the init process (will not run yet)
	Process init = { .name = "init\0", .processID = 1, .processState = 0, .priority = 6, 
			 .parent = NULL, .children = NULL, .olderSibling = NULL, .youngerSibling = NULL};

	// because of the moduulo rule, we need to make the index 1 here
	table[1] = init;	
	tableOccupancies[1] = 1;

	//TODO more things
}

void TEMP_switchTo(int pid) {
    /* TODO - temporary context switch fxn
	   before doing the context switch, switch curr to the new process!
	   (do the same thing in dispatcher in phase 1b)
	 */

	USLOSS_Context* oldContext = currentProcess->context;

	for (int i = 0; i < MAXPROC; i++) {
		if (table[i].processID == pid) {
			currentProcess = &table[i];
			break;
		}
	}

	USLOSS_ContextSwitch(oldContext, currentProcess->context);
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
   	Process newProcess = { .name = name, .processID = processIDCounter, .processState = 0, .priority = priority, .context = NULL, 
			       .parent = currentProcess, .children = NULL, .olderSibling = currentProcess->children, .youngerSibling = NULL};
   	
	// add process into table and link with parent and older sibling
	table[processIDCounter % MAXPROC] = newProcess;
	tableOccupancies[processIDCounter % MAXPROC] = 1;
	if (currentProcess->children != NULL) {
		currentProcess->children->youngerSibling = &table[processIDCounter % MAXPROC];
	}
	currentProcess->children = &table[processIDCounter % MAXPROC];
	
	// spec says to allocate this but not sure what to do with it
	void *stack = malloc(stackSize);

	// initialize the USLOSS_Context
	USLOSS_ContextInit(newProcess.context, stack, stackSize, NULL, &processWrapper);

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

/*
  Wrapper function for process main functions. Requires setting the new
  process that is about to be run as the current process.

  Arguments: None
  Returns: Void
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
	// Uncomment once we start using the testcases; the definitions are in there. For now it just causes a compile error. 
	// phase2_start_service_processes();
	// phase3_start_service_processes();
	// phase4_start_service_processes();
	// phase5_start_service_processes();

	spork("testcase_main", &testcase_main, NULL, USLOSS_MIN_STACK, 3);

	/* TODO
	   create testcase_main (need to use spork once it's done)
	   enter join loop (need completed join)
	   if join returns an error, terminate the program
	*/
}
