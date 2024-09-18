/*
 * These are the definitions for structures related to processes 
 */

#ifndef _PROCESS_H_
#define _PROCESS_H_

#include "../include/usloss.h"

/*
 * Represents the state of the CPU at a given moment in time
 */
typedef struct CPUState {
	
	// TODO not yet sure how to structure this

} CPUState;

typedef struct SchedulingInformation {

	// TODO not yet sure how to structure this

} SchedulingInformation;

typedef struct AccountingInformation {
	
	// TODO not yet sure how to structure this

} AccountingInformation;

/*
 * Represents an individual process 
 */
typedef struct Process {

	char* name;
	int processID;

	// CPUState* cpuState;
	int processState;
	int priority;

	// char* memory; // might require a custom struct, not sure yet

	//SchedulingInformation* schedulingInformation;
	// AccountingInfromation* accountingInformation;
	
	// ??? openFiles;
	// ??? otherResources;

	USLOSS_Context* context;

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