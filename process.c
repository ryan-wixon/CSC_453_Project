/* 
 * process.c
 * Ryan Wixon and Adrianna Koppes
 * CSC 452
 * 
 * Implements the functions defined in process.c in order ot manipulate Process structs
 */

#include <stdio.h>
#include <string.h>
#include "process.h"

/*
 * Prints information for a given process, used for debugging
 *
 * Arguments: process = A pointer to the process to print information about
 *   Returns: Void 
 */
void printProcess(Process* process) {
	
	// print the PID, must align correctly
	if (process->processID < 10) {
		printf("   %d", process->processID);
	}
	else if (process->processID < 100) {
		printf("  %d", process->processID);
	}
	else {
		printf(" %d", process->processID);
	}

	// print the PPID, also must align correctly
	if (process->parent != NULL) {
		if (process->parent->processID < 10) {
			printf("     %d", process->parent->processID);
		}
		else if (process->parent->processID < 100){
			printf("    %d", process->parent->processID);
		}
		else {
			printf("   %d", process->parent->processID);
		}
	}
	else {
		printf("     0");
	}

	// print the process name
	printf("  %s", process->name);
	
	// align the text pointer to where it needs to be based on the name length
	int nameLength = strlen(process->name);
	for (int i = 0; i < 18 - nameLength; i++) {
		printf(" ");
	}
	
	// print the priority, yet again must alighn correctly
	if (process->priority < 10) {
		printf("%d         ", process->priority);
	}
	else {
		printf("%d        ", process->priority);
	}	

	// print process status
	switch(process->processState) {
		case -1:
			printf("Terminated(%d)\n", process->exitStatus);
			break;
		case 0:
			printf("Runnable\n");
			break;
		case 1:
			printf("Running\n");
			break;
		case 2:
			printf("Blocked");
			if(process->childDeathWait == 1) {
				/* waiting for child to die */
				printf("(waiting for child to quit)\n");
			}
			else if(process->zapWait == 1) {
				/* waiting for zap target to die */
				printf("(waiting for zap target to quit)\n");
			}
			else {
				/* we are blocked for some other reason */
				printf("(%d)\n", process->parent->processID);
			}
			break;
		default:
			printf("Unknown\n");
	}
}

/*
 * Adds a process to a given RunQueue
 *
 * Arguments: queue = the RunQueue to be added to; addProcess = the process to add
 *   Returns: Void 
 */
void addToRunQueue(RunQueue* queue, Process* addProcess) {

	if (queue->oldest == NULL) {
		queue->newest = addProcess;
		queue->oldest = addProcess;
	}
	else {
		queue->newest->nextInQueue = addProcess;
		addProcess->prevInQueue = queue->newest;
		queue->newest = addProcess;
	}
}

/*
 * Pops a process from a given RunQueue
 *
 * Arguments: queue = the RunQueue to remove the front element from
 *   Returns: The process popped from the RunQueue
 */
Process* popFromRunQueue(RunQueue* queue) {
	
	Process* retval = queue->oldest;
	if (retval->prevInQueue == NULL && retval->nextInQueue == NULL) {
		
		// only value in the queue
		queue->oldest = NULL;
		queue->newest = NULL;
		return retval;
	}
	queue->oldest = queue->oldest->nextInQueue;
	queue->oldest->prevInQueue = NULL;
	retval->prevInQueue = NULL;
	retval->nextInQueue = NULL;
	
	return retval;
}

/*
 * Pops a process from a given RunQueue and adds it to the back
 *
 * Arguments: queue = the RunQueue to modify
 *   Returns: The process sent to the back of the RunQueue
 */
Process* sendToBackRunQueue(RunQueue* queue) {
	
	Process* retval = popFromRunQueue(queue);
	addToRunQueue(queue, retval);
	return retval;
}
