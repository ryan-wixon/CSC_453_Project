#include <stdio.h>
#include "process.h"

/*
 * Prints information for a given process, used for debugging
 */
void printProcess(Process* process) {

	printf("         Name: %s\n", process->name);
	printf("          PID: %d\n", process->processID);
	
	// print CPU state information here
	printf("Process State: %d\n", process->processState); // TODO once we've assigned states to integers we should just print out a string constant here to make it more convenient
	printf("     Priority: %d\n", process->priority);

	// print memory information here

	// print scheduling information here
	// print accounting information here

	// print open files information here
	// print other resources information here
	
	if (process->parent != NULL) {
		printf("  Parent Name: %s\n", process->parent->name);
		printf("   Parent PID: %d\n", process->parent->processID);
	}

	int childNum = 0;
	Process* currentChild = process->children;
	while (currentChild != NULL) {
		printf(" Child %d Name: %s\n", childNum, currentChild->name);
		printf("  Child %d PID: %d\n", childNum, currentChild->processID);
		currentChild = currentChild->olderSibling;
	}
}
