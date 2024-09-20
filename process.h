/*
 * These are the definitions for structures related to processes 
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
		
	int(*processMain)(void *);	/* start function for the process */
	void* mainArgs;			/* args for the process main function */

	struct Process* parent;
	struct Process* children;
	struct Process* olderSibling;
	struct Process* youngerSibling;
		
} Process;

/*
 * Prints information for a given process, used for debugging
 */
void printProcess(Process* process);

#endif
