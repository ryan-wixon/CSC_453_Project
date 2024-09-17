#include <stdio.h>
#include <stdlib.h>
#include "phase1.h"
#include "process.h"

/* the process table; note there is
one extra slot in the table, which is so the first index in the array
is empty, allowing init to be the first process in the table. */
Process table[MAXPROC + 1];     
int tableOccupancies[MAXPROC + 1];

int nextOpenSlot = 1;   /* stores next open index in process table */
Process *curr = NULL;      /* the current running process */

void phase1_init() {
    
	// set every table entry to vacant
	for (int i = 1; i < MAXPROC + 1; i++) {
		tableOccupancies[i] = 0;
	}

	//TODO more things

}

void TEMP_switchTo(int pid) {
    /* TODO - temporary context switch fxn */
}

int spork(char *name, int(*func)(void *), void *arg, int stacksize, int priority) {
    /* TODO - fork esque fxn
    Russ's instructions for the function:
    - Look for a slot in the process table and assign an ID (numProcesses + 1)
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
	printf("Process Dump\n------------------------------\n");
	for (int i = 1; i < MAXPROC + 1; i++) {
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

// just using for testing for now
int main() {

	phase1_init();

	Process testProc = { .name = "Process1\0", .processID = 1, .processState = 0, .priority = 3, .parent = NULL, .children = NULL, .olderSibling = NULL, .youngerSibling = NULL};
	table[1] = testProc;
	tableOccupancies[1] = 1;

	dumpProcesses();
	return 0;
}
