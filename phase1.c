/* 
 * phase1.c
 * Ryan Wixon and Adrianna Koppes
 * CSC 452
 * 
 * TODO: header comment (maybe?)
 */

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

Process *currentProcess = NULL; 	/* the current running process */

char initStack[USLOSS_MIN_STACK];	/* stack for init, must allocate on startup */

void phase1_init() {

	printf("ENTERING: phase1_init()\n");
    
	// set every table entry to vacant
	for (int i = 0; i < MAXPROC; i++) {
		tableOccupancies[i] = 0;
	}
	/* null out the entire table to prepare for filling */
	memset(table, 0, sizeof(table));

	// create the init process (will not run yet)
	Process init = { .name = "init\0", .processID = 1, .processState = 0, .priority = 6, 
			 .processMain = &initProcessMain, .mainArgs = NULL, 
			 .parent = NULL, .children = NULL, .olderSibling = NULL, .youngerSibling = NULL
		       };
	
	// because of the moduulo rule, we need to make the index 1 here
	table[1] = init;	
	tableOccupancies[1] = 1;

	// initialize context for init process
	USLOSS_ContextInit(&table[1].context, initStack, USLOSS_MIN_STACK, NULL, initProcessMain);
	
	//TODO more things - maybe?
	
	printf("EXITING: phase1_init()\n");
}

void TEMP_switchTo(int pid) {
    /* TODO - temporary context switch fxn
	   before doing the context switch, switch curr to the new process!
	   (do the same thing in dispatcher in phase 1b)
	 */

	printf("ENTERING: TEMP_switchTo(%d)\n", pid);

	USLOSS_Context* oldContext = NULL;	
	if (currentProcess != NULL) {
		oldContext = &currentProcess->context;
	}
	
	for (int i = 0; i < MAXPROC; i++) {
		if (tableOccupancies[i] == 1 && table[i].processID == pid) {
			currentProcess = &table[i];
			break;
		}
	}
	
	USLOSS_ContextSwitch(oldContext, &currentProcess->context);

	printf("EXITING: TEMP_switchTo(%d)\n", pid);
}

int spork(char *name, int(*func)(void *), void *arg, int stackSize, int priority) {
    /* TODO - fork esque fxn
    Russ's instructions for the function:
    - Look for a slot in the process table and assign an ID (numProcesses + 1)
    - Fill in the details of the process
    - Set up the context structure so that we can (later) context switch to the new process
    - PART 1B ONLY: Call the dispatcher to see if it wants to do a context switch
    */

	printf("TRYING TO SPORK %s\n", name);

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
			       .processMain = func, .mainArgs = arg, 
			       .parent = currentProcess, .children = NULL, .olderSibling = currentProcess->children, .youngerSibling = NULL 
			     };
		
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
	USLOSS_ContextInit(&newProcess.context, stack, stackSize, NULL, &processWrapper);

	printf("SPORKED NEW PROCESS %s WITH PID %d\n", name, newProcess.processID);

	return newProcess.processID;
}

int join(int *status) {
	if (status == NULL) {
		/* invalid argument */
		return -3;
	}
	else if (currentProcess->children == NULL) {
		/* the process has no children */
		return -2;
	}
	
	// check for dead children, return the first one found
	Process* curr = currentProcess->children;
	while (curr != NULL) {
		if (curr->processState == -1) {
			*status = curr->exitStatus;
			/* reset relationships between dead process and live processes */
			if(curr->processID == currentProcess->children->processID) {
				if(curr->olderSibling == NULL) {
					/* all children have died */
					currentProcess->children = NULL;
				}
				else {
					/* make sure the child list still connects to parent */
					currentProcess->children = curr->olderSibling;
					curr->olderSibling->youngerSibling = NULL;
				}
			}
			else if(curr->olderSibling == NULL) {
				/* oldest process in the child list */
				curr->youngerSibling->olderSibling = NULL;
			}
			else {
				/* delete from middle of child list */
				curr->olderSibling->youngerSibling = curr->youngerSibling;
				curr->youngerSibling->olderSibling = curr->olderSibling;
			}
			/* sever ties to related processes so no potential dangling pointers */
			curr->youngerSibling = NULL;
			curr->olderSibling = NULL;
			curr->parent = NULL;
			/* free the dead process's memory */
			free(curr->context->stack);
			memset(table[curr->processID % MAXPROC], 0, sizeof(curr));
			tableOccupancies[curr->processID % MAXPROC] = 0;
			return curr->processID;
		}
		curr =  curr->olderSibling;  
	}
	
	// if a parent has no dead children, it blocks to wait for them to die
	// this won't happen in phase1a, so here's a dummy return value (should never see this returned)
    	return -1;
}

void quit_phase_1a(int status, int switchToPid) {
    
	currentProcess->processState = -1;
	currentProcess->exitStatus = status;

	if(strcmp(currentProcess->name, "testcase_main") == 0) {
		/* testcase_main has terminated; halt the simulation */
		/* we use the name to find testcase_main, since it doesn't have
		   a designated PID */
		if(status != 0) {
			fprintf(stderr, "The simulation has encountered an error with the error code %d and will now terminate.\n", status);
		}
		USLOSS_Halt(status);
	}
	
	TEMP_switchTo(switchToPid);
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
	/* for now, in Phase 1A, we don't have to worry about quitting on behalf 
	   of the process if it just returns. So if it happens, for now, just
	   switch to init because the process to switch to cannot be known. */
	quit_phase_1a(endStatus, 1);
}

/* 
 * The function that includes the init process's functionality. It 
 * starts all the necessary service processes, then creates the 
 * testcase_main process, before finally entering a (basically) infinite
 * loop calling join() until there are no more child processes left. Once
 * that happens, the simulation concludes.
 * 
 * Arguments: None.
 * Returns: None.
 */
void initProcessMain() {
	
	printf("ENTERING: initProcessMain()\n");	

	/* call service processes for other phases (for now these are NOPs ) */
	// phase2_start_service_processes();
	// phase3_start_service_processes();
	// phase4_start_service_processes();
	// phase5_start_service_processes();

	// create testcase main and switch to it (in phase1b, only call spork)
	TEMP_switchTo(spork("testcase_main", testcase_main, NULL, USLOSS_MIN_STACK, 3));

	/* 
	   enter join loop (need completed join)
	   if join returns an error, terminate the program
	*/
	int endStatus = 1;
	int processStatus = 0;	// keep track of process status

	printf("ENTERING JOIN LOOP\n");
	while(endStatus != -2) {
		endStatus = join(&processStatus);
		//printf("RETURNED %d\n", endStatus);
	}
	
	// loop exited...no more children! (this should never be reached)
	fprintf(stderr, "Error: All child processes have been terminated. The simulation will now conclude.\n");
	exit(endStatus);
}
