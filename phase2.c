/*
 * phase2.c
 * Ryan Wixon and Adrianna Koppes
 * documentation TODO
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <phase1.h>  /* to access phase 1 functions like blockMe() */
#include "phase2.h"

typedef struct Messsage {

	char* message;			// the raw bytes of the message; should not be treated as a string
	int messageLength;		// the length of the message
	
	struct Message* nextMessage; 	// messages can form lists inside mailboxes
 
} Message;

typedef struct Mailbox {

	Message* slots;		// linked list of messages stored in this mailbox
	Message* lastMessage; 	// last message stored in this mailbox; used for easy appends
	int numSlots;		// total number of available slots
	int filledSlots;	// number of slots that are occupied

	//TODO not sure if this should be void*, these should be linked lists of processes
	void* producers;	// processes that want to write to this mailbox line up here
	void* consumers;	// processes that want to read from this mailbox line up here
	
} Mailbox;

// global array of mailboxes; a mailbox's ID corresponds to it's index in this array
Mailbox mailboxes[MAXMBOX];
int mailboxOccupancies[MAXMBOX];
int numMailboxes = 0;

int send(int mboxID, void *message, int msgSize, int doesBlock);
int receive(int mboxID, void *message, int maxMsgSize, int doesBlock);
void phase2_start_service_processes();
void nullsys(void);
void clockHandler(int type, void *arg);
void diskHandler(int type, void *arg);
void terminalHandler(int type, void *arg);
// TODO IMPLEMENT INTERRUPT HANDLERS (see below)

// TODO ADD THE OTHER REQUIRED DATA STRUCTURES
// vector of system calls - set every single element to nullsys (see below)
void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);

void phase2_init(void) {
    unsigned int oldPSR = USLOSS_PsrGet();
    if(oldPSR % 2 == 0) {
        // we cannot call phase2_init in user mode
        fprintf(stderr, "ERROR: Someone attempted to call phase2_init while in user mode!\n");
        USLOSS_Halt(1);
    }
    if(oldPSR % 4 == 3) {
        // interrupts enabled...we need to disable them
        if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
            fprintf(stderr, "Bad PSR set in phase2_init\n");
        }
    }

	memset(mailboxes, 0, sizeof(mailboxes));
	memset(mailboxes, 0, sizeof(mailboxOccupancies));

    // install the interrupt handlers
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;
    USLOSS_IntVec[USLOSS_TERM_INT] = terminalHandler;
    USLOSS_IntVec[USLOSS_DISK_INT] = diskHandler;

    // TODO - implement the rest of phase2_init

    // restore old PSR
    if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
        fprintf(stderr, "Bad PSR restored in phase2_init\n");
    }
}

void phase2_start_service_processes() {
    // TODO - may end up being an NOP but we are required to have it
    // this is where we start any service processes that we need
    // but it's unlikely we will have them.
}

void waitDevice(int type, int unit, int *status) {
    // TODO
}

void wakeupByDevice(int type, int unit, int status) {
    // NOP - as confirmed on Discord, we are not implementing this
    // but it's in the phase2.h file, so we have to include it.
    printf("NOP -- wakeupByDevice is not supposed to be implemented for this semester.\n");
}

void clockHandler(int type, void *arg) {
    // TODO
}

void diskHandler(int type, void *arg) {
    // TODO
}

void terminalHandler(int type, void *arg) {
    // TODO
}

/*
 * Handler for system calls. We are not working with syscalls yet
 * (wait until Phase 3), so this "handler" simply prints an error
 * message and terminates the simulation if a syscall is detected.
 * 
 * Arguments: None.
 * Returns: None.
 */
void nullsys(void) {
    // TODO - maybe want to disable interrrupts? not sure.
    fprintf(stderr, "ERROR: System call detected. There should be no syscalls yet.\n");
    USLOSS_Halt(1);
}

int MboxCreate(int slots, int slot_size) {

	if (slots < 0 || slot_size < 0 || slots > MAXSLOTS || slot_size > MAX_MESSAGE) {
		fprintf(stderr, "ERROR: Bad arguments given to MboxCreate.\n");
		return -1;
	}
    
	Mailbox newMailbox;
	
	newMailbox.slots = NULL;
	newMailbox.numSlots = slots;
	newMailbox.filledSlots = 0;

	newMailbox.producers = NULL;
	newMailbox.consumers = NULL;

	// try to copy the new mailbox into a free spot, return the index if successful.
	for (int i = 0; i < MAXMBOX; i++) {
		if (mailboxes[i] != NULL) {
			mailboxes[i] = newMailbox;
			return i;	
		}
	}

	// don't do anything if there is no space for the mailbox
	fprintf(stderr, "ERROR: No space to create new mailbox.\n");

	return -1;
}

int MboxRelease(int mbox_id) {
	
	if (mailboxes[mbox_id] != NULL) {
		free(mailboxes[mbox_id].slots);
		mailboxes[mbox_id] = NULL;

		//TODO More stuff probably
		return 0;
	}
	
	fprintf(stderr, "ERROR: Attempting to release a mailbox which does not exist.\n")
	return -1;
}

int MboxSend(int mbox_id, void *msg_ptr, int msg_size) {

	// package the bytes of the message into a new message
	Message newMessage;
	newMessage.message = (char*)msg_ptr;
	newMessage.messageLength = msg_size;
	newMessage.nextMessage = NULL;
	
	// now try to put the message into 
	if (mailboxes[mbox_id].filledSlots < mailboxes[mbox_id].numSlots) {
		mailboxes[mbox_id].lastMessage->nextMessage = 
	}
	else {
		// there is no slot for the message, the sender needs to join the producer queue and block
		// TODO actually do that
	}
}

int MboxRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    // TODO
    return -1; // dummy return value! replace!!
}

int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {
    // TODO
    return -1;  // dummy return value! replace!!
}

int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    // TODO
    return -1;   // dummy return value! replace!!
}

/*
 * Helper function that sends a message and either blocks or returns
 * a special value if the send fails. Basically, it's both MboxSend
 * and MboxCondSend in one, and both of those functions call this one.
 * Since they both basically do the same thing, it was recommended in
 * class that we just have a helper function that does both and then
 * call this helper function from MboxSend and MboxCondSend, with the
 * parameter doesBlock giving us guidance on whether we should just
 * block or return a value but do not block when send fails.
 * 
 * Arguments: TODO
 * Returns: TODO
 */
int send(int mboxID, void *message, int msgSize, int doesBlock) {
    // TODO
    return -1;  // dummy return value! replace!!
}

/* 
 * Helper function that receives a message and either blocks or
 * returns a special value if the receive fails. Basically, it's both
 * MboxRecv and MboxCondRecv in one, and both of those functions call
 * this one. Since both of those functions do essentially the same
 * thing, it was recommended in class that we have a helper function
 * that does both and then call this helper function from both MboxRecv
 * and MboxCondRecv, with the parameter doesBlock giving guidance on
 * whether to block or return a value but do not block when receiving
 * fails.
 * 
 * Arguments: TODO
 * Returns: TODO
 */
int receive(int mboxID, void *message, int maxMsgSize, int doesBlock) {
    // TODO
    return -1;  // dummy return value! replace!!
}
