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

/* Information for processes in Phase 2 is included here, since we can't
   add to the Phase 1 process table. */
typedef struct Process2 {
	
	int pid;  		       // process ID, to match to ensure if a process dies, we can find out

	int blocked;		       // 0 = Runnable; 1 = Blocked;	

	struct Process2* nextProducer; // next process in a send queue for mailbox
	struct Process2* nextConsumer; // next process in a receive queue for mailbox
	struct Process2* prevProducer; // previous process in a send queue for mailbox
	struct Process2* prevConsumer; // previous process in a receive queue for mailbox

} Process2;

typedef struct Message {
	
	int occupied;			// indicates whether or not this is an active message; do not read any other fields if it is 0	

	char message[MAX_MESSAGE];	// the raw bytes of the message; should not be treated as a string
	int messageLength;		// the length of the message

	int index;			// the index of the message in the global array

	struct Message* nextMessage; 	// messages can form lists inside mailboxes
	struct Message* prevMessage;
 
} Message;

typedef struct Mailbox {
	
	int occupied;		// indicates whether or not this is an active mailbox; do not read any other fields if it is 0
	
	Message* messageSlots;	// linked list of messages stored in this mailbox
	Message* lastMessage; 	// last message stored in this mailbox; used for easy appends
	int numSlots;		// total number of available slots
	int filledSlots;	// number of slots that are occupied

	Process2* producers;	// processes that want to write to this mailbox line up here
	Process2* consumers;	// processes that want to read from this mailbox line up here
	Process2* lastProducer;	// last producer stored in this mailbox's queue; used for easy appends
	Process2* lastConsumer; // last consumer stored in this mailbox's queue; used for easy appends
	int numProducers;	// the number of queued producers
	int numConsumers;	// the number of queued consumers

} Mailbox;

/* index of each of the interrupt handler mailboxes in the mailbox array */
#define CLOCK_INDEX          0
#define DISK_1_INDEX         1
#define DISK_2_INDEX         2
#define TERM_1_INDEX         3
#define TERM_2_INDEX         4
#define TERM_3_INDEX         5
#define TERM_4_INDEX         6

int time = 0;	// for delay interrupt

// global array of mailboxes; a mailbox's ID corresponds to it's index in this array
Mailbox mailboxes[MAXMBOX];

// global array of message slots 
Message messages[MAXSLOTS];

// shadow process table for Phase 2
Process2 procTable2[MAXPROC];

// vector of system calls - set every single element to nullsys (see below)
void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);

int send(int mboxID, void *message, int msgSize, int doesBlock);
int receive(int mboxID, void *message, int maxMsgSize, int doesBlock);
void phase2_start_service_processes();
void nullsys(void);
void clockHandler(int type, void *arg);
void diskHandler(int type, void *arg);
void terminalHandler(int type, void *arg);
void syscallHandler(int type, void *arg);
// TODO IMPLEMENT INTERRUPT HANDLERS (see below) -- need to wait for mailbox implementation

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

	// will zero out all mailbox memory, including the occupied field (no mailboxes will be active)
	memset(mailboxes, 0, sizeof(mailboxes));

	// install the interrupt handlers
	USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;
	USLOSS_IntVec[USLOSS_TERM_INT] = terminalHandler;
	USLOSS_IntVec[USLOSS_DISK_INT] = diskHandler;
	USLOSS_IntVec[USLOSS_SYSCALL_INT] = syscallHandler;

	// interrupt handler mailboxes - there will be 7 mailboxes corresponding to 1 interrupt each
	// interrupts are not yet implemented but I want to run certain testcases now
	MboxCreate(1, sizeof(int));
	MboxCreate(1, sizeof(int));
	MboxCreate(1, sizeof(int));
	MboxCreate(1, sizeof(int));
	MboxCreate(1, sizeof(int));
	MboxCreate(1, sizeof(int));
	MboxCreate(1, sizeof(int));

	// get current time to update for next clock interrupt
	time = currentTime();
	
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

// may not work.
void waitDevice(int type, int unit, int *status) {
	// disable interrupts
	unsigned int oldPSR = USLOSS_PsrGet();
    if(oldPSR % 2 == 0) {
        // we cannot call waitDevice in user mode
        fprintf(stderr, "ERROR: Someone attempted to call waitDevice while in user mode!\n");
        USLOSS_Halt(1);
    }
    if(oldPSR % 4 == 3) {
        // interrupts enabled...we need to disable them
        if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
            fprintf(stderr, "Bad PSR set in waitDevice\n");
        }
    }

	int index = -1;		// index of mailbox to receive from

	// recv from different mailbox based on device type and unit number
	if(type == USLOSS_CLOCK_DEV) {
		// wait on clock interrupt
		if(unit != 0) {
			fprintf(stderr, "ERROR: Invalid device unit number.\n");
        	USLOSS_Halt(1);
		}
		index = 0;
	}
	else if(type == USLOSS_DISK_DEV) {
		// wait on disk interrupt
		if(unit != 0 && unit != 1) {
			fprintf(stderr, "ERROR: Invalid device unit number.\n");
        	USLOSS_Halt(1);
		}
		index = unit + DISK_1_INDEX;
	}
	else if(type == USLOSS_TERM_DEV) {
		// wait on terminal interrupt
		if(unit < 0 || unit > 3) {
			fprintf(stderr, "ERROR: Invalid device unit number.\n");
        	USLOSS_Halt(1);
		}
		index = unit + TERM_1_INDEX;
	}
	else {
		// should never get here, but just in case.
		fprintf(stderr, "ERROR: Invalid device type.\n");
        USLOSS_Halt(1);
	}

	// attempt to receive from the proper device's mailbox - it will block, but
	// once a message is sent, MboxRecv will wake up and the message will be
	// delivered here.
	MboxRecv(index, status, sizeof(int));

	// restore old PSR
    if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
        fprintf(stderr, "Bad PSR restored in waitDevice\n");
    }
}

void wakeupByDevice(int type, int unit, int status) {
    // NOP - as confirmed on Discord, we are not implementing this
    // but it's in the phase2.h file, so we have to include it.
    printf("NOP -- wakeupByDevice is not supposed to be implemented for this semester.\n");
}

// may not work.
void clockHandler(int type, void *arg) {
	// do not disable interrupts. USLOSS will do it for us.
	// get current time, see if it has been 100 ms
	// NOTE: as per Discord, current time is retrieved in microseconds!
	if(currentTime() - time >= 100000) {
		// enough time has passed; send another message to processes waiting
		// on delay
		int status = 0;
		int requestStatus = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &status);
		if (requestStatus == USLOSS_DEV_INVALID) {
			// should never reach here, this is more for debugging.
			fprintf(stderr, "ERROR: Invalid arguments used for USLOSS_DeviceInput in clockHandler.\n");
			USLOSS_Halt(1);
		}
		// send message
		MboxCondSend(CLOCK_INDEX, &status, sizeof(int));

		// set time of the last context switch
		time = currentTime();

		// call the dispatcher when control returns to the interrupt handler
		dispatcher();
	}
}

// may not work.
void diskHandler(int type, void *arg) {
	// do not disable interrupts. USLOSS will do it for us.
	int unitNo = (int)(long)arg;	// get unit number of disk
	// get device index
	int index = DISK_1_INDEX + unitNo;
	int status = 0;
	int requestStatus = USLOSS_DeviceInput(USLOSS_DISK_DEV, unitNo, &status);
	if(requestStatus == USLOSS_DEV_INVALID) {
		// should never reach here, this is more for debugging.
		fprintf(stderr, "ERROR: Invalid arguments used for USLOSS_DeviceInput in diskHandler.\n");
		USLOSS_Halt(1);
	}
	// send message
	MboxCondSend(index, &status, sizeof(int));
}

// may not work.
void terminalHandler(int type, void *arg) {
	// do not disable interrupts. USLOSS will do it for us.
	int unitNo = (int)(long)arg;	// get unit number of terminal
	// get device index
	int index = TERM_1_INDEX + unitNo;
	int status = 0;
	int requestStatus = USLOSS_DeviceInput(USLOSS_TERM_DEV, unitNo, &status);
	if(requestStatus == USLOSS_DEV_INVALID) {
		// should never reach here, this is more for debugging.
		fprintf(stderr, "ERROR: Invalid arguments used for USLOSS_DeviceInput in terminalHandler.\n");
		USLOSS_Halt(1);
	}
	// send message
	MboxCondSend(index, &status, sizeof(int));
}

// TODO uncomment and possibly fix once we get everything else working.
void syscallHandler(int type, void *arg) {
	// do not disable interrupts. USLOSS will do it for us.
	//USLOSS_Sysargs callArgs = (USLOSS_Sysargs*)arg;	// may need to fix syntax here
	//int index = callArgs->number;
	//systemCallVec[index](void);
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
    // do not disable interrupts. all we do is halt.
    fprintf(stderr, "ERROR: System call detected. There should be no syscalls yet.\n");
    USLOSS_Halt(1);
}

int MboxCreate(int slots, int slot_size) {
	
	// disable/restore interrupts
	unsigned int oldPSR = USLOSS_PsrGet();
	if (oldPSR % 2 == 0) {
		
		// we cannot call this function in user mode
		fprintf(stderr, "ERROR: Someone attempted to call MboxCreate while in user mode!\n");
		USLOSS_Halt(1);
	}
	if (oldPSR % 4 == 3) {
	
	// interrupts enabled...we need to disable them
	if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in MboxCreate\n");
		}
	}

	// basic error checking for arguments
	if (slots < 0 || slot_size < 0 || slots > MAXSLOTS || slot_size > MAX_MESSAGE) {
		return -1;
	}

	// try to create a mailbox in a new slot, return the index if successful.
	for (int i = 0; i < MAXMBOX; i++) {
		if (mailboxes[i].occupied == 0) {
			
			mailboxes[i].occupied = 1;
			
			mailboxes[i].messageSlots = NULL;
			mailboxes[i].numSlots = slots;
			mailboxes[i].filledSlots = 0;
			
			mailboxes[i].producers = NULL;
			mailboxes[i].consumers = NULL;
			mailboxes[i].lastProducer = NULL;
			mailboxes[i].lastConsumer = NULL;
			mailboxes[i].numProducers = 0;
			mailboxes[i].numConsumers = 0;
				
			// restore old PSR
			if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
				fprintf(stderr, "Bad PSR restored in MboxCreate\n");
			}
			return i;	
		}
	}

	// restore old PSR
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in MboxCreate\n");
	}
	return -1;
}

int MboxRelease(int mbox_id) {
	
	// disable/restore interrupts
	unsigned int oldPSR = USLOSS_PsrGet();
	if (oldPSR % 2 == 0) {

		// we cannot call this function in user mode
		fprintf(stderr, "ERROR: Someone attempted to call MboxRelease while in user mode!\n");
		USLOSS_Halt(1);
	}
	if (oldPSR % 4 == 3) {

		// interrupts enabled...we need to disable them
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in MboxRelease\n");
		}
	}
	
	if (mailboxes[mbox_id].occupied == 1) {
		
		// remove all existing messages in this mailbox's queue
		Message* curr = mailboxes[mbox_id].messageSlots;
		while (curr != NULL) {
			curr->occupied = 0;
			curr = curr->nextMessage;
		}

		// mark the mailbox as released; it's not done just yet but it needs to appear dead so that the
		// processes in the queue don't try to do anything when we wake them up
		mailboxes[mbox_id].occupied = 0;

		// unblock all processes that are currently waiting in the queues
		Process2* curr2 = mailboxes[mbox_id].producers;
		while (curr2 != NULL) {
			if (curr2->blocked == 1) {
				unblockProc(curr2->pid);
				curr2->blocked = 0;
			}
			curr2 = curr2->nextProducer;
		}
		curr2 = mailboxes[mbox_id].consumers;
		while (curr2 != NULL) {
			if (curr2->blocked == 1) {
				unblockProc(curr2->pid);
				curr2->blocked = 0;
			}
			curr2 = curr2->nextConsumer;
		}
	
		// restore old PSR
		if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR restored in MboxRelease\n");
		}
		return 0;
	}
	
	// restore old PSR
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in MboxRelease\n");
	}
	return -1;
}

void addToProducerQueue(int mbox_id, int currentPID) {

	if (mailboxes[mbox_id].producers == NULL) {
		mailboxes[mbox_id].producers = &procTable2[currentPID % MAXPROC];
	}
	else {
		mailboxes[mbox_id].lastProducer->nextProducer = &procTable2[currentPID % MAXPROC];
		mailboxes[mbox_id].lastProducer->nextProducer->prevProducer = mailboxes[mbox_id].lastProducer;
	}
	mailboxes[mbox_id].lastProducer = &procTable2[currentPID % MAXPROC];

	mailboxes[mbox_id].numProducers++;
}

void removeFromProducerQueue(int mbox_id, int currentPID) {

	mailboxes[mbox_id].producers = procTable2[currentPID % MAXPROC].nextProducer;
	if (mailboxes[mbox_id].producers == NULL) {
		mailboxes[mbox_id].lastProducer = NULL;
	}
	mailboxes[mbox_id].numProducers--;
}

void addToConsumerQueue(int mbox_id, int currentPID) {

	if (mailboxes[mbox_id].consumers == NULL) {
		mailboxes[mbox_id].consumers = &procTable2[currentPID % MAXPROC];
	}
	else {
		mailboxes[mbox_id].lastConsumer->nextConsumer = &procTable2[currentPID % MAXPROC];
		mailboxes[mbox_id].lastConsumer->nextConsumer->prevConsumer = mailboxes[mbox_id].lastConsumer;
	}
	mailboxes[mbox_id].lastConsumer = &procTable2[currentPID % MAXPROC];

	mailboxes[mbox_id].numConsumers++;
}

void removeFromConsumerQueue(int mbox_id, int currentPID) {

	mailboxes[mbox_id].consumers = procTable2[currentPID % MAXPROC].nextConsumer;
	if (mailboxes[mbox_id].consumers == NULL) {
		mailboxes[mbox_id].lastConsumer = NULL;
	}
	mailboxes[mbox_id].numConsumers--;
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
int send(int mbox_id, void *msg_ptr, int msg_size, int doesBlock) {
    	
	// basic error checking for arguments
	if (mailboxes[mbox_id].occupied == 0) {
		return -1;
	}

	// special case for zero slot mailboxes
	if (mailboxes[mbox_id].numSlots == 0) {
		
		// try and find a blocked consumer
		Process2* curr = mailboxes[mbox_id].consumers;
		while (curr != NULL && curr->blocked == 0) {
			curr = curr->nextConsumer;
		}	
		
		// if there is no consumer waiting we need to block, if there is we need to wake it up 
		if (curr == NULL) {
		
			int currentPID = getpid();
			procTable2[currentPID % MAXPROC].pid = currentPID;
			procTable2[currentPID % MAXPROC].blocked = 1;
			procTable2[currentPID % MAXPROC].nextProducer = NULL;
			procTable2[currentPID % MAXPROC].nextConsumer = NULL;
			procTable2[currentPID % MAXPROC].prevProducer = NULL;
			procTable2[currentPID % MAXPROC].prevConsumer = NULL;

			addToProducerQueue(mbox_id, getpid());
			blockMe();
			removeFromProducerQueue(mbox_id, getpid());
		}
		else {
			curr->blocked = 0;
			unblockProc(curr->pid);
		}
		return 0;
	}

	// check to see if the sender can fill a slot right away
	int enqueued = 0;
	int currentPID = getpid();
	if (mailboxes[mbox_id].filledSlots == mailboxes[mbox_id].numSlots) {
		
		// past this point, the process will block; if this is a nonblocking operation we must return here
		if (doesBlock == 1) {
			return -2;
		}

		// there is not yet a slot for this message, so then sender needs to join the producer queue and block; the sender
		// must not wake up until it is guaranteed that they will have a slot to put their message into
		procTable2[currentPID % MAXPROC].pid = currentPID;
		procTable2[currentPID % MAXPROC].blocked = 1;
		procTable2[currentPID % MAXPROC].nextProducer = NULL;
		procTable2[currentPID % MAXPROC].nextConsumer = NULL;
		procTable2[currentPID % MAXPROC].prevProducer = NULL;
		procTable2[currentPID % MAXPROC].prevConsumer = NULL;

		addToProducerQueue(mbox_id, currentPID);

		enqueued = 1; 
		blockMe();

		// if the mailbox got released while the sender was asleep, it needs to just return -1
		if (mailboxes[mbox_id].occupied == 0) {
			return -1;
		}
	}
	
	// try to create a message in a new slot
	int messageIndex = -1;
	for (int i = 0; i < MAXSLOTS; i++) {
		if (messages[i].occupied == 0) {
			
			messages[i].occupied = 1;

			memcpy(&messages[i].message, msg_ptr, msg_size);
			messages[i].messageLength = msg_size;
		
			messages[i].index = i;
	
			messages[i].nextMessage = NULL;
		
			messageIndex = i;
			break;
		}
	}
	if (messageIndex == -1) {
		return -2;
	}

	// now try to put the message into the queue of the target mailbox
	if (mailboxes[mbox_id].messageSlots == NULL) {
		mailboxes[mbox_id].messageSlots = &messages[messageIndex];
	}
	else {
		mailboxes[mbox_id].lastMessage->nextMessage = &messages[messageIndex];
		mailboxes[mbox_id].lastMessage->nextMessage->prevMessage = mailboxes[mbox_id].lastMessage; 
	}
	mailboxes[mbox_id].lastMessage = &messages[messageIndex];
	mailboxes[mbox_id].filledSlots++;	
	
	// if there are consumers waiting, wake up the first (if it has not already been woken up)
	if (mailboxes[mbox_id].consumers != NULL && mailboxes[mbox_id].consumers->blocked == 1) {
		unblockProc(mailboxes[mbox_id].consumers->pid);
	} 

	// if the process was enqueued, it needs to be taken out of the queue
	if (enqueued == 1) {
		removeFromProducerQueue(mbox_id, currentPID);
	}
	return 0; 
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
int receive(int mbox_id, void *msg_ptr, int msg_max_size, int doesBlock) {

	// basic error checking for arguments
	if (mailboxes[mbox_id].occupied == 0) {
		return -1;
	}

	// special case for zero slot mailboxes
	if (mailboxes[mbox_id].numSlots == 0) {
		
		// try and find a blocked producer
		Process2* curr = mailboxes[mbox_id].producers;
		while (curr != NULL && curr->blocked == 0) {
			curr = curr->nextProducer;
		}	
		
		// if there are no producers waiting we need to block, if there are we need to wake it up 
		if (curr == NULL) {
		
			int currentPID = getpid();
			procTable2[currentPID % MAXPROC].pid = currentPID;
			procTable2[currentPID % MAXPROC].blocked = 1;
			procTable2[currentPID % MAXPROC].nextProducer = NULL;
			procTable2[currentPID % MAXPROC].nextConsumer = NULL;
			procTable2[currentPID % MAXPROC].prevProducer = NULL;
			procTable2[currentPID % MAXPROC].prevConsumer = NULL;

			addToConsumerQueue(mbox_id, currentPID);
			blockMe();
			removeFromConsumerQueue(mbox_id, currentPID);
		}
		else {
			curr->blocked = 0;
			unblockProc(curr->pid);
		}
		return 0;
	}

	// check to see if the reciever can read a message right away
	if (mailboxes[mbox_id].filledSlots > mailboxes[mbox_id].numConsumers) {
		
		// iterate to the message that belongs to the consumer
		Message* curr = mailboxes[mbox_id].messageSlots;
		for (int i = 0; i < mailboxes[mbox_id].numConsumers; i++) {
			curr = curr->nextMessage;
		}
	
		// make sure the message is not to long for the consumer to read
		if (curr->messageLength > msg_max_size) {
			fprintf(stderr, "ERROR: Message is too long for the recipient to read");
			return -1;
		}

		// copy the raw bytes from the message into the recipient's buffer
		memcpy(msg_ptr, curr->message, curr->messageLength);

		// remove the message from the queue
		if (curr->prevMessage != NULL) {
			curr->prevMessage->nextMessage = curr->nextMessage;
		}
		else {
			mailboxes[mbox_id].messageSlots = curr->nextMessage;
		}
		if (curr->nextMessage != NULL) {
			curr->nextMessage->prevMessage = curr->prevMessage;
		}
		curr->occupied = 0;
		mailboxes[mbox_id].filledSlots--;
		
		// wake up producers if they are waiting (save the message length first in case the producer overwrites it)
		int retval = curr->messageLength;
		if (mailboxes[mbox_id].producers != NULL) {
			unblockProc(mailboxes[mbox_id].producers->pid);
		}
		return retval;
	}

	// past this point, the process will block; if this is a nonblocking operation we must return here
	if (doesBlock == 1) {
		return -2;
	}

	// if we couldn't read a message immediately, the recipient must join the consumer queue and block; the recipient
	// must not wake up until it is guaranteed that they will have a slot to put their message into.
	int currentPID = getpid();
	procTable2[currentPID % MAXPROC].pid = currentPID;
	procTable2[currentPID % MAXPROC].blocked = 1;
	procTable2[currentPID % MAXPROC].nextProducer = NULL;
	procTable2[currentPID % MAXPROC].nextConsumer = NULL;
	procTable2[currentPID % MAXPROC].prevProducer = NULL;
	procTable2[currentPID % MAXPROC].prevConsumer = NULL;

	addToConsumerQueue(mbox_id, currentPID); 
	blockMe();

	// find out what position the recipient is in the consumer queue after waking up, then get the corresponding message
	int position = 0;
	Process2* curr = mailboxes[mbox_id].consumers;
	while (curr->pid != getpid()) {
		position++;
		curr = curr->nextConsumer;
	}

	Message* targetMessage = mailboxes[mbox_id].messageSlots;
	for (int i = 0; i < position; i++) {
		targetMessage = targetMessage->nextMessage;
	}

	// copy the raw bytes from the message into the recipient's buffer and return the bytes read
	memcpy(msg_ptr, targetMessage->message, targetMessage->messageLength);
	
	messages[targetMessage->index].occupied = 0;
	mailboxes[mbox_id].filledSlots--;
	return targetMessage->messageLength;
}

int MboxSend(int mbox_id, void *msg_ptr, int msg_size) {
	
	// disable/restore interrupts
	unsigned int oldPSR = USLOSS_PsrGet();
    	if (oldPSR % 2 == 0) {
        	
		// we cannot call this function in user mode
        	fprintf(stderr, "ERROR: Someone attempted to call MboxSend while in user mode!\n");
        	USLOSS_Halt(1);
    	}
    	if (oldPSR % 4 == 3) {
        	
		// interrupts enabled...we need to disable them
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in MboxSend\n");
		}
    	}

	int retval = send(mbox_id, msg_ptr, msg_size, 0);
	
	// restore old PSR
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in MboxSend\n");
	}
	return retval;
}

int MboxRecv(int mbox_id, void *msg_ptr, int msg_max_size) {
    
	// disable/restore interrupts
	unsigned int oldPSR = USLOSS_PsrGet();
    	if (oldPSR % 2 == 0) {
        	
		// we cannot call this function in user mode
        	fprintf(stderr, "ERROR: Someone attempted to call MboxRecv while in user mode!\n");
        	USLOSS_Halt(1);
    	}
    	if (oldPSR % 4 == 3) {
        	
		// interrupts enabled...we need to disable them
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in MboxRecv\n");
		}
    	}

	int retval = receive(mbox_id, msg_ptr, msg_max_size, 0);

	// restore old PSR
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in MboxRecv\n");
	}
	return retval;
}

int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {

	// disable/restore interrupts
	unsigned int oldPSR = USLOSS_PsrGet();
    	if (oldPSR % 2 == 0) {
        	
		// we cannot call this function in user mode
        	fprintf(stderr, "ERROR: Someone attempted to call MboxCondSend while in user mode!\n");
        	USLOSS_Halt(1);
    	}
    	if (oldPSR % 4 == 3) {
        	
		// interrupts enabled...we need to disable them
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in MboxCondSend\n");
		}
    	}

	int retval = send(mbox_id, msg_ptr, msg_size, 1);
	
	// restore old PSR
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in MboxCondSend\n");
	}
	return retval;
}

int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_max_size) {

	// disable/restore interrupts
	unsigned int oldPSR = USLOSS_PsrGet();
    	if (oldPSR % 2 == 0) {
        	
		// we cannot call this function in user mode
        	fprintf(stderr, "ERROR: Someone attempted to call MboxCondRecv while in user mode!\n");
        	USLOSS_Halt(1);
    	}
    	if (oldPSR % 4 == 3) {
        	
		// interrupts enabled...we need to disable them
		if (USLOSS_PsrSet(oldPSR - 2) == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Bad PSR set in MboxCondRecv\n");
		}
    	}

	int retval = receive(mbox_id, msg_ptr, msg_max_size, 1);

	// restore old PSR
	if (USLOSS_PsrSet(oldPSR) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR restored in MboxCondRecv\n");
	}
	return retval;
}
