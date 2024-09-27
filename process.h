/*
 * process.h
 * Ryan Wixon and Adriana Koppes 
 * CSC 452
 *
 * Defines a struct for representing an indiviudal process and several methods for manipulating them 
 */

#ifndef _PROCESS_H_
#define _PROCESS_H_

#include "../include/usloss.h"

/*
 * Represents an individual process 
 */
typedef struct Process {

	char* name;
	int processID;

	int processState;		/* determines what state the process is in (running, blocked, etc.) */
	int exitStatus;			/* value set after process quits; cannot be safely read beforehand */
	int priority;			

	USLOSS_Context context;
	void* contextStack; 
		
	int (*processMain) (void *);	/* start function for the process */
	void* mainArgs;			/* args for the process main function */

	struct Process* parent;
	struct Process* children;
	struct Process* olderSibling;
	struct Process* youngerSibling;

	struct Process* nextInQueue;	/* for use in run queues */
		
} Process;

/* 
 * Represents a run queue of priority given by the array index
 */
typedef struct RunQueue {
	struct Process* newest;	/* process most recently added to run queue */
	struct Process* oldest; /* earliest process in run queue */
} RunQueue;

/*
 * Prints information for a given process, used for debugging
 *
 * Arguments: process = A pointer to the process to print information about
 *   Returns: Void 
 */
void printProcess(Process* process);

#endif
