/* 
 * phase1.c
 * Ryan Wixon and Adrianna Koppes
 * CSC 452
 * 
 * Implements the functions defined in phase1.h in order to simulate an operating system kernel.
 * This phase deals with process control and ensuring that the operating system can create new
 * processes, run them, switch to new processes, and then destroy them when they finish.
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

/* as per spec, need separate run queue for each priority */
RunQueue runQueues[7];	/* index 0 is not used, only 1-6 */

/*
 * Startup code for phase 1. Initializes necessary data structures for process
 * handling and creates the init process.
 *
 * Arguments: None
 *   Returns: Void
 */
void phase1_init() {
	
	unsigned int oldPSR = USLOSS_PsrGet();
	if(oldPSR % 2 == 0) {
		
		// you cannot call phase1_init() in user mode
		fprintf(stderr, "ERROR: Someone attempted to call phase1_init while in user mode!\n");
		USLOSS_Halt(1);
	}
	if (oldPSR == 3) {
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in phase1_init\n");
		}
	}
 
	// set every table entry to vacant
	for (int i = 0; i < MAXPROC; i++) {
		tableOccupancies[i] = 0;
	}

	// set up the run queues (recall: index 0 never used)
	for(int j = 1; j < 7; j++) {
		RunQueue queue = { .newest = NULL, .oldest = NULL };
		runQueues[j] = queue;
	}
	
	// null out the entire table to prepare for filling
	memset(table, 0, sizeof(table));

	// create the init process (will not run yet)
	Process init = { .name = "init\0", .processID = 1, .processState = 0, .priority = 6, 
			 .childDeathWait = 0, .zapWait = 0, 
			 .processMain = initProcessMain, .mainArgs = NULL, 
			 .parent = NULL, .children = NULL, .olderSibling = NULL, .youngerSibling = NULL, 
			 .zappers = NULL, .nextZapper = NULL, .nextInQueue = NULL, .prevInQueue = NULL
		       };
	
	// because of the moduulo rule, we need to make the index 1 here
	table[1] = init;	
	tableOccupancies[1] = 1;
	runQueues[6]->newest = init;	// put init in its specified run queue
	runQueues[6]->oldest = init;

	// initialize context for init process
	USLOSS_ContextInit(&table[1].context, initStack, USLOSS_MIN_STACK, NULL, processWrapper);
	
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in phase1_init\n");
	}
}

/*
 * Temporary context switching function for phase 1a. Switches to the
 * specified process. The new process is also set as the current process.
 * 
 * TODO: Adapt this code into the code for dispatcher (see below). We will
 * also need to adapt the code to take run queues into account.
 * This function will eventually be deleted. But don't delete it until we
 * have dispatcher() written.
 *
 * Arguments: pid = The PID of the process to switch to
 *   Returns: Void
 */
void TEMP_switchTo(int pid) {
	
	unsigned int oldPSR = USLOSS_PsrGet();
	if(oldPSR % 2 == 0) {
		
		// you cannot call TEMP_switchTo() in user mode
		fprintf(stderr, "ERROR: Someone attempted to call TEMP_switchTo while in user mode!\n");
		USLOSS_Halt(1);
	}
	if (oldPSR == 3) {
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in TEMP_switchTo\n");
		}
	}

	Process *oldProcess = currentProcess;
	
	// find the process to switch to
	for (int i = 0; i < MAXPROC; i++) {
		if (tableOccupancies[i] == 1 && table[i].processID == pid) {
			currentProcess = &table[i];
			break;
		}
	}

	// perform the switch
	if (oldProcess == NULL) {

		// set the new process to running
		currentProcess->processState = 1;
		USLOSS_ContextSwitch(NULL, &currentProcess->context);
	}
	else {

		// update the old process state if it hasn't just terminated, and set the new process to running
		if (oldProcess->processState == 1) {
			oldProcess->processState = 0;
		} 
		currentProcess->processState = 1;
		USLOSS_ContextSwitch(&oldProcess->context, &currentProcess->context);
	}
	
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in TEMP_switchTo\n");
	}
}

/*
 * Creates a new child of the currently running process
 *
 * Arguments: name = The name of the process to be created; func = A function pointer to the process' main method;
	      arg = A pointer to the process' main method arguments; stackSize = The size of the process' stack space, in bytes;
	      priority = An integer representing the process' starting priority 
 *   Returns: Integer representing the PID of the newly created process
 */
int spork(char *name, int(*func)(void *), void *arg, int stackSize, int priority) {

	// check to ensure we are in user mode with interrupts disabled
	unsigned int oldPSR = USLOSS_PsrGet();
	if(oldPSR % 2 == 0) {
		// you cannot call spork() in user mode
		fprintf(stderr, "ERROR: Someone attempted to call spork while in user mode!\n");
		USLOSS_Halt(1);
	}
	if (oldPSR == 3) {
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in spork\n");
		}
	}

	// check for errors; return -2 if stack size is too small and -1 for all else
	if (stackSize < USLOSS_MIN_STACK) {
		if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR restored in spork\n");
		}
		return -2;
	}
	if (numProcesses == MAXPROC || priority < 0 || priority > 7 || func == NULL || name == NULL || strlen(name) > pow(MAXNAME, 7)) {
		if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR restored in spork\n");
		}
		return -1;
	}

	// find the next process ID to use and create process
	while (tableOccupancies[processIDCounter % MAXPROC] != 0) {
		processIDCounter++;
	}
   	Process newProcess = { .name = name, .processID = processIDCounter, .processState = 0, .priority = priority, 
			       .childDeathWait = 0, .zapWait = 0, 
				   .processMain = func, .mainArgs = arg, 
			       .parent = currentProcess, .children = NULL, .olderSibling = currentProcess->children, .youngerSibling = NULL, 
				   .zappers = NULL, .nextZapper = NULL, .nextInQueue = NULL, .prevInQueue = NULL
			     };
		
	// add process into table and link with parent and older sibling
	int newProcessIndex = processIDCounter % MAXPROC;
	table[newProcessIndex] = newProcess;
	tableOccupancies[newProcessIndex] = 1;
	if (currentProcess->children != NULL) {
		currentProcess->children->youngerSibling = &table[newProcessIndex];
		table[newProcessIndex].olderSibling = currentProcess->children;
	}
	currentProcess->children = &table[newProcessIndex];
	
	// make stack for a process (used in USLOSS_Context)
	void *stack = malloc(stackSize);

	// initialize the USLOSS_Context and save the pointer to the stack
	USLOSS_ContextInit(&table[newProcessIndex].context, stack, stackSize, NULL, &processWrapper);
	newProcess.contextStack = stack;

	// add the process to the given run queue
	if(runQueues[priority]->newest == NULL && runQueues[priority]->oldest == NULL) {
		// new process is the first one in the queue
		runQueues[priority]->newest = newProcess;
		runQueues[priority]->oldest = newProcess;
	}
	else {
		// add new process to existing queue
		newProcess->nextInQueue = runQueues[priority]->newest;
		runQueues[priority]->newest->prevInQueue = newProcess;
		runQueues[priority]->newest = newProcess;
	}
	
	// ensure processes never repeat IDs
	numProcesses++;
	processIDCounter++;

	// possibly run the child if it is high enough priority
	dispatcher();

	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in spork\n");
	}
	return newProcess.processID;
}

/*
 * Finds and returns the first dead child of the current process, or blocks if all children are alive
 *
 * Arguments: status = A pointer to an integer to write the dead child's return status to 
 *   Returns: Integer representing the PID of the dead child collected
 */
int join(int *status) {
	
	unsigned int oldPSR = USLOSS_PsrGet();
	if(oldPSR % 2 == 0) {
		
		// you cannot call join() in user mode
		fprintf(stderr, "ERROR: Someone attempted to call join while in user mode!\n");
		USLOSS_Halt(1);
	}
	if (oldPSR == 3) {
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in join\n");
		}

	}

	
	if (status == NULL) {
		
		// invalid argument
		if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR restored in join\n");
		}
		return -3;
	}
	if (currentProcess->children == NULL) {
		
		// the process has no children		
		if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR restored in join\n");
		}
		return -2;
	}
	
	// check for dead children, return the first one found
	Process* curr = currentProcess->children;
	while (curr != NULL) {
		if (curr->processState == -1) {
			*status = curr->exitStatus;
			
			// reset relationships between dead process and live processes
			if (curr->processID == currentProcess->children->processID) {
				if (curr->olderSibling == NULL) {
					
					// all children have died
					currentProcess->children = NULL;
				}
				else {
					// make sure the child list still connects to parent
					currentProcess->children = curr->olderSibling;
					curr->olderSibling->youngerSibling = NULL;
				}
			}
			else if (curr->olderSibling == NULL) {
				
				// oldest process in the child list
				curr->youngerSibling->olderSibling = NULL;
			}
			else {
				// delete from middle of child list
				curr->olderSibling->youngerSibling = curr->youngerSibling;
				curr->youngerSibling->olderSibling = curr->olderSibling;
			}
			
			// sever ties to related processes so no potential dangling pointers
			curr->youngerSibling = NULL;
			curr->olderSibling = NULL;
			curr->parent = NULL;
			
			// free the dead process's memory
			free(curr->contextStack);
			tableOccupancies[curr->processID % MAXPROC] = 0;
			
			if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
				fprintf(stderr, "Bad PSR restored in join\n");
			}

			numProcesses--;
			return curr-> processID;
		}
		curr = curr->olderSibling;  
	}
	
	// parent has no dead children - block to wait for them to die
	currentProcess->childDeathWait = 1;
	blockMe();

	// reset PSR to old value
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in join\n");
	}
}

/* 
 * Temportary quit function for phase 1a 
 * 
 * TODO: Remove this function once we know for sure that quit() works
 *
 * Arguments: status = The return status for the current process; switchToPid = The PID of the process to manually switch to
 *   Returns: Void
 */
void quit_phase_1a(int status, int switchToPid) {
    
	unsigned int oldPSR = USLOSS_PsrGet();
	if(oldPSR % 2 == 0) {

		// you cannot call quit() in user mode
		fprintf(stderr, "ERROR: Someone attempted to call quit_phase_1a while in user mode!\n");
		USLOSS_Halt(1);
	}
	if (oldPSR == 3) {
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in quit_phase_1a\n");
		}

	}

	if(currentProcess->children != NULL) {
		fprintf(stderr, "ERROR: Process pid %d called quit() while it still had children.\n", currentProcess->processID);
		USLOSS_Halt(1);
	}

	currentProcess->processState = -1;
	currentProcess->exitStatus = status;

	if(strcmp(currentProcess->name, "testcase_main") == 0) {
		/* testcase_main has terminated; halt the simulation */
		/* we use the name to find testcase_main, since it doesn't have
		   a designated PID */
		if(status != 0) {
			fprintf(stderr, "The simulation has encountered an error with the error code %d and will now terminate.\n", status);
		}
		if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR restored in quit_phase_1a\n");
		}
		USLOSS_Halt(status);
	}
	
	TEMP_switchTo(switchToPid);
	
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in quit_phase_1a\n");
	}
}

// TODO: test this function! I think it is done but not sure.
void quit(int status) {
	/* check PSR to ensure we are in kernel mode; disable interrupts */
	unsigned int oldPSR = USLOSS_PsrGet();
	if(oldPSR % 2 == 0) {

		// you cannot call quit() in user mode
		fprintf(stderr, "ERROR: Someone attempted to call quit while in user mode!\n");
		USLOSS_Halt(1);
	}
	if (oldPSR == 3) {
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in quit\n");
		}

	}

	/* you cannot call quit() when you still have children */
	if(currentProcess->children != NULL) {
		fprintf(stderr, "ERROR: Process pid %d called quit() while it still had children.\n", currentProcess->processID);
		USLOSS_Halt(1);
	}

	/* show current process has terminated */
	currentProcess->processState = -1;
	currentProcess->exitStatus = status;

	if(strcmp(currentProcess->name, "testcase_main") == 0) {
		/* testcase_main has terminated; halt the simulation */
		/* we use the name to find testcase_main, since it doesn't have
		   a designated PID like init does */
		if(status != 0) {
			fprintf(stderr, "The simulation has encountered an error with the error code %d and will now terminate.\n", status);
		}
		if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR restored in quit\n");
		}
		USLOSS_Halt(status);
	}

	/* if parent blocked waiting for child to die, inform it */
	Process *parent = currentProcess->parent;
	if(parent->processState == 2 && parent->childDeathWait == 1) {
		currentProcess->parent->childDeathWait = 0;
		unblockProc(parent->processID);
	}

	/* if zappers waiting for process to die, inform it */
	Process *currZapper = currentProcess->zappers;
	while(currZapper != NULL) {
		if(currZapper->processState == 2 && currZapper->zapWait == 1) {
			currentProcess->zappers = currZapper->nextZapper;
			currZapper->nextZapper = NULL;
			unblockProc(currZapper->processID);
		}
		currZapper = currentProcess->zappers;	/* move to next zapper */
	}

	/* if we still haven't switched to new process, call dispatcher */
	dispatcher();

	/* restore old PSR */
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in quit\n");
	}
}

void zap(int pid) {
	// check PSR, disable interrupts if needed
	unsigned int oldPSR = USLOSS_PsrGet();
	if(oldPSR % 2 == 0) {

		// you cannot call zap() in user mode
		fprintf(stderr, "ERROR: Someone attempted to call zap while in user mode!\n");
		USLOSS_Halt(1);
	}
	if (oldPSR == 3) {
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in zap\n");
		}

	}

	/* TODO - implement remaining zap functionality */

	// block and wait for the zap target to die (it cannot 
	// do anything to make the target die)
	currentProcess->zapWait = 1;
	blockMe();

	// reset PSR to its previous value, possibly restoring interrupts
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in zap\n");
	}
}

void blockMe(void) {
	/* check PSR to ensure we are in kernel mode; disable interrupts */
	unsigned int oldPSR = USLOSS_PsrGet();
	if(oldPSR % 2 == 0) {

		// you cannot call blockMe() in user mode
		fprintf(stderr, "ERROR: Someone attempted to call blockMe while in user mode!\n");
		USLOSS_Halt(1);
	}
	if (oldPSR == 3) {
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in blockMe\n");
		}

	}

	/* TODO - implement actual functionality of blockMe */

	// reset PSR to its previous value, possibly restoring interrupts
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in blockMe\n");
	}
}

int unblockProc(int pid) {
	// check PSR, disable interrupts if needed
	unsigned int oldPSR = USLOSS_PsrGet();
	if(oldPSR % 2 == 0) {

		// you cannot call unblockProc() in user mode
		fprintf(stderr, "ERROR: Someone attempted to call unblockProc while in user mode!\n");
		USLOSS_Halt(1);
	}
	if (oldPSR == 3) {
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in unblockProc\n");
		}

	}

	/* TODO - implement actual functionality of unblockProc */

	// reset PSR to its previous value, possibly restoring interrupts
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in unblockProc\n");
	}

	return -1;	// remove this when implementing functionality! temp return.
}

void dispatcher(void) {
	// check PSR, disable interrupts if needed
	unsigned int oldPSR = USLOSS_PsrGet();
	if(oldPSR % 2 == 0) {

		// you cannot call dispatcher() in user mode
		fprintf(stderr, "ERROR: Someone attempted to call dispatcher while in user mode!\n");
		USLOSS_Halt(1);
	}
	if (oldPSR == 3) {
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in dispatcher\n");
		}

	}

	/* TODO - actual implementation of dispatcher code */

	// reset PSR to its previous value, possibly restoring interrupts
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in dispatcher\n");
	}
}

int currentTime(void) {
	/* TODO - ask about this since it's not in the spec at all */
	return -1 // remove this! temp return.
}

/* 
 * Returns the PID of the current process.
 *
 * Arguments: None
 *   Returns: Integer representing the PID of the current process.
 */
int getpid() {
	unsigned int oldPSR = USLOSS_PsrGet();
	if(oldPSR % 2 == 0) {
		// you cannot call getpid() in user mode
		fprintf(stderr, "ERROR: Someone attempted to call getpid while in user mode!\n");
		USLOSS_Halt(1);
	}
	return currentProcess->processID;
}

/*
 * Prints the current process table.
 *
 * Arguments: None
 *   Returns: Void 
 */
void dumpProcesses() {
	
	unsigned int oldPSR = USLOSS_PsrGet();
	if(oldPSR % 2 == 0) {
		
		// you cannot call dumpProcesses() in user mode
		fprintf(stderr, "ERROR: Someone attempted to call dumpProcesses while in user mode!\n");
		USLOSS_Halt(1);
	}
	if (oldPSR == 3) {
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in dumpProcesses\n");
		}
	}

	printf(" PID  PPID  NAME              PRIORITY  STATE\n");
	for (int i = 0; i < MAXPROC; i++) {
		if (tableOccupancies[i] == 1) {
			printProcess(&table[i]);
		}
	}
	
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in dumpProcesses\n");
	}
}

/*
 * Wrapper function for process main functions. Requires setting the new
 * process that is about to be run as the current process. Runs the current
 * process.
 *
 * Arguments: None
 *   Returns: Void
 */
void processWrapper() {
	
	// enable interrupts before switching into new process
	unsigned int oldPSR = USLOSS_PsrGet();
	
	// processes may run in user mode, so don't halt if you're in user mode here
	if (oldPSR == 1) {
		if (USLOSS_PsrSet(oldPSR + 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in processWrapper\n");
		}
	}

	/* before context switching, MUST change current process to new process!*/
	int endStatus = currentProcess->processMain(currentProcess->mainArgs);

	/* special handling: if process just returns and doesn't quit explicitly, 
	   we call quit on behalf of the process */
	quit(endStatus);

	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in processWrapper\n");
	}

}

/*
 * The second wrapper function for testcase_main, necessary because
 * the regular testcase_main function (in the testcases) does not have
 * an argument, but our process table function requires the use of a
 * main function with an argument. It just calls testcase_main() and
 * then returns the testcase's return code.
 * 
 * Arguments: void pointer ignored, which is an unused argument.
 * Returns: integer representing the return code of testcase_main
 */
int testcaseMainWrapper(void* ignored) {
	int returnCode = testcase_main();
	return returnCode;
}

/* 
 * The function that includes the init process's functionality. It 
 * starts all the necessary service processes, then creates the 
 * testcase_main process, before finally entering a (basically) infinite
 * loop calling join() until there are no more child processes left. Once
 * that happens, the simulation concludes.
 * 
 * Arguments: void pointer ignored, which is an unused argument.
 *   Returns: integer representing the return code of init (but in 
 *   practicality, should never actually return).
 */
int initProcessMain(void* ignored) {
	
	unsigned int oldPSR = USLOSS_PsrGet();
	if(oldPSR % 2 == 0) {
		
		// you cannot run init in user mode
		fprintf(stderr, "ERROR: Someone attempted to run init while in user mode!\n");
		USLOSS_Halt(1);
	}
	if (oldPSR == 3) {
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in initProcessMain\n");
		}

	}

	// init is running; take it off the run queue for priority 6
	runQueues[6]->newest = NULL;
	runQueues[6]->oldest = NULL;

	// call service processes for other phases (for now these are NOPs)
	phase2_start_service_processes();
	phase3_start_service_processes();
	phase4_start_service_processes();
	phase5_start_service_processes();

	printf("Phase 1A TEMPORARY HACK: init() manually switching to testcase_main() after using spork() to create it.\n");

	// create testcase main and switch to it (in phase1b, only call spork)
	TEMP_switchTo(spork("testcase_main", testcaseMainWrapper, NULL, USLOSS_MIN_STACK, 3));

	// enter join loop; if it returns an error quit the program
	int endStatus = 1;
	int processStatus = 0;	// keep track of process status
	while(endStatus != -2) {
		endStatus = join(&processStatus);
	}
	
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in initProcessMain\n");
	}

	// loop exited...no more children! (this should never be reached)
	fprintf(stderr, "Error: All child processes have been terminated. The simulation will now conclude.\n");
	USLOSS_Halt(1);

	return -1;	// should never be reached
}
