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

	int processState;
	int priority;

	USLOSS_Context context;
	
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
