#include <stdio.h>
#include <string.h>
#include "process.h"

/*
 * Prints information for a given process, used for debugging
 */
void printProcess(Process* process) {

	if (process->processID < 10) {
		printf("   %d", process->processID);
	}
	else {
		printf("  %d", process->processID);
	}

	if (process->parent != NULL) {
		if (process->parent->processID < 10) {
			printf("     %d", process->parent->processID);
		}
		else {
			printf("    %d", process->parent->processID);
		}
	}
	else {
		printf("     0");
	}

	printf("  %s", process->name);
	
	int nameLength = strlen(process->name);
	for (int i = 0; i < 18 - nameLength; i++) {
		printf(" ");
	}
	
	if (process->priority < 10) {
		printf("%d         ", process->priority);
	}
	else {
		printf("%d        ", process->priority);
	}

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
		default:
			printf("Unknown\n");
	}
}
