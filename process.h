/*
 * process.h
 * Ryan Wixon and Adriana Koppes 
 * CSC 452
 *
 * Defines a struct for representing an indiviudal process and several methods for manipulating them.
 * Also defines a struct for representing a run queue and multiple constants for determining 
 * process status. 
 */

#ifndef _PROCESS_H_
#define _PROCESS_H_

#include "../include/usloss.h"

/* Process overall statuses */
/* 
 * Status of a terminated process
 */
#define TERMINATED      -1

/* 
 * Status of a runnable (i.e., not blocked) process
 */
#define RUNNABLE      0

/* 
 * Status of the currently running process
 */
#define RUNNING      1

/* 
 * Status of a blocked process
 */
#define BLOCKED      2

/*
 * Represents an individual process 
 */
typedef struct Process {

	char* name;
	int processID;

	int processState;		/* determines what state the process is in (running, blocked, etc.) */
	int exitStatus;			/* value set after process quits; cannot be safely read beforehand */
	int priority;			
	int childDeathWait;		/* 1 if process waiting for child to die to join; 0 otherwise */
	int zapWait;			/* 1 if process waiting for zap target to die; 0 otherwise */
	int zapped;			/* 1 if process is being zapped by 1 or more processes; 0 otherwise */

	USLOSS_Context context;
	void* contextStack; 
		
	int (*processMain) (void *);	/* start function for the process */
	void* mainArgs;			/* args for the process main function */

	struct Process* parent;
	struct Process* children;
	struct Process* olderSibling;
	struct Process* youngerSibling;

	struct Process* zappers;	/* all the processes that called zap() on this one */
	struct Process* nextZapper;	/* next process in a list of zappers */

	struct Process* nextInQueue;	/* for use in run queues */
	struct Process* prevInQueue;    /* for use in run queues */
		
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
