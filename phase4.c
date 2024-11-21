/*
 * Ryan Wixon and Adrianna Koppes
 * Phase 4a
 * CSC 452-001
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <usyscall.h> /* for system call constants, etc. */
#include <phase1.h>   /* to access phase 1 functions, if necessary */
#include <phase2.h>   /* to access phase2 functions, for mailboxes */
#include <phase3.h>
#include <phase3_kernelInterfaces.h> /* for semaphore functionality */
//#include <phase3_usermode.h>
#include <phase4.h>

void phase4_start_service_processes();

/* for queueing processes for sleeping/waking */
typedef struct SleepProc {

    int pid;          /* ID of the process that is sleeping */
    long wakeTime;    /* time at which the process should wake up (in microseconds) */
    int wakeBox;      /* ID of the mailbox used to wake the process */
    SleepProc *next;  /* next process in the queue */
} SleepProc;

/* for queuing processes for writing to the terminal */
typedef struct WriteProc {
    int pid;   /* ID of the process writing to the terminal */
    char *buf;  /* buffer to write */
    int bufLength;  /* buffer length */
    int curr;    /* index of next char to write */
    int wakeBox;  /* mailbox to wake the process up */
    WriteProc *next;  /* next process in the queue */
} WriteProc;

int sleepLock = -1;   /* lock for Sleep handler - can no longer use global lock */
int readLock = -1;    /* lock for reading from terminal */
int writeLock = -1;   /* lock for writing to terminal */

// Phase 4b -- add locks for the other interrupts

SleepProc sleepQueue = NULL; /* head of the sleep queue */
int numSleeping = 0;  /* number of processes waiting to sleep */

// mailbox IDs for mailboxes used as read queues
int readQueue0;
int readQueue1;
int readQueue2;
int readQueue3;

/* head of each write queue */
WriteProc writeQueue0 = NULL;
WriteProc writeQueue1 = NULL;
WriteProc writeQueue2 = NULL;
WriteProc writeQueue3 = NULL;

void phase4_init(void) {
    
    // create locks
    sleepLock = MboxCreate(1, 0);
    readLock = MboxCreate(1, 0);
    writeLock = MboxCreate(1, 0);

    // create read mailboxes (one for each terminal)
    readQueue0 = MboxCreate(10, MAXLINE + 1);
    readQueue1 = MboxCreate(10, MAXLINE + 1);
    readQueue2 = MboxCreate(10, MAXLINE + 1);
    readQueue3 = MboxCreate(10, MAXLINE + 1);

    // TODO - maybe.
}

void phase4_start_service_processes() {
    // start daemons -- will all have max priority.
    int (*sleepFunc)(void*) = sleepDaemon;
    int (*termFunc0)(void*) = terminalDaemon;
    int (*termFunc1)(void*) = terminalDaemon;
    int (*termFunc2)(void*) = terminalDaemon;
    int (*termFunc3)(void*) = terminalDaemon;
    spork("sleepDaemon", sleepFunc, (void*)(long)0, USLOSS_MIN_STACK, 1);
    spork("termDaemon0", termFunc0, (void*)(long)0, USLOSS_MIN_STACK, 1);
    spork("termDaemon1", termFunc1, (void*)(long)1, USLOSS_MIN_STACK, 1);
    spork("termDaemon2", termFunc2, (void*)(long)2, USLOSS_MIN_STACK, 1);
    spork("termDaemon3", termFunc3, (void*)(long)3, USLOSS_MIN_STACK, 1);

    // TODO -- start daemons
    //   - read terminal daemon (the way I think this is supposed to work is, 
    // you have a daemon reading terminal input all the time, sending the 
    // buffers it reads in to a mailbox. Mailbox can only store 10 buffers max. 
    // Use MboxCondSend to send the buffers, so when it reaches 10, it'll just
    // stop and all other data is discarded as described in the spec. Then, when
    // TermRead is called, all we do is just receive from the mailbox and get the
    // next available buffer. Use regular MboxRecv because the TermRead syscall
    // is supposed to block if there is no available input.)
}

/* Phase 4a system call handlers */

void sleep(USLOSS_Sysargs *args) {
    
    getLock(sleepLock);
    int waitTime = (int)(long)args->arg1;
    if(waitTime < 0) {
        
        // illegal wait time!
        args->arg4 = (void*)(long)-1;
        releaseLock(sleepLock);
        return;
    }

    // valid wait time (includes 0 -- it will just be put at front of queue)
    numSleeping++;

    // recall: USLOSS clock gives microseconds!!
    long wakeInterval = currentTime() + (waitTime * 1000000);
    SleepProc curr = sleepQueue;
    SleepProc prev = sleepQueue;
    while(curr != NULL) {
        if(curr.wakeTime > wakeInterval) {
            // found our curr and prev values
            break;
        }
        if(prev == sleepQueue) {
            curr = curr->next;
        }
        else {
            curr = curr->next;
            prev = prev->next;
        }
    }

    // add new process to the queue
    int toWake = MboxCreate(1, 0);
    SleepProc sleeping = {.pid = getpid(), .wakeBox = toWake, 
                    .wakeTime = wakeInterval, .next = NULL};
    prev->next = sleeping;

    // prepare args for return.
    args->arg4 = (void*)(long)0;
    
    // put the process to sleep
    releaseLock(sleepLock);
    MboxRecv(toWake, NULL, 0);

    // control returns to the current process -- it can now continue.
}

void termRead(USLOSS_Sysargs *args) {
    getLock(readLock);
    char *buffer = (char*)args->arg1;   // possibly need to fix syntax here
    int bufSize = (int)(long)args->arg2;
    int unit = (int)(long)args->arg3;
    // check for invalid input and return if needed
	if (bufSize < 0 || bufSize > MAXLINE || unit < 0 || unit > 3) {
		args->arg4 = (void*)(long)-1;
		return;
	}
	else {
		args->arg4 = (void*)(long)0;
	}

    int readQueue = -1;

    // figure out which terminal to read to
    if(unit == 0) {
        readQueue = readQueue0;
    }
    else if(unit == 1) {
        readQueue = readQueue1;
    }
    else if(unit == 2) {
        readQueue = readQueue2;
    }
    else {
        // we already filtered out invalid terminal units
        readQueue = readQueue3;
    }

    // receive from the read mailbox.
    int size = 0;
    releaseLock(readLock);
    size = MboxRecv(readQueue, buffer, bufSize);

    getLock(readLock);
    args->arg2 = (void*)(long)size;
    return;
}

/*
void termRead(char* buffer, int bufSize, int unit, int *lenOut) {
	
	// check for invalid input and return if needed
	if (bufSize < 0 || bufSize > MAXLINE || unit < 0 || unit > 3) {
		//args->arg4 = (void*)(long)-1;
		return;
	}
	else {
		//args->arg4 = (void*)(long)0;
	}

	// do we need to grab a lock for this?
	getLock(readLock);

	int charactersRead = 0;
	//args->arg2 = (void*)(long)charactersRead;

	releaseLock(readLock);	
}
*/

void termWrite(USLOSS_Sysargs *args) {
    getLock(writeLock);
    char *buffer = (char*)args->arg1;
    int bufSize = (int)(long)args->arg2;
    int unit = (int)(long)args->arg3;
    if (bufSize < 0 || bufSize > MAXLINE || unit < 0 || unit > 3) {
		args->arg4 = (void*)(long)-1;
		return;
	}
    else {
		args->arg4 = (void*)(long)0;
	}

    // we know we can write to the terminal since our arguments are valid
    WriteProc *queue = NULL;
    // figure out which write queue to enter
    if(unit == 0) {
        queue = writeQueue0;
    }
    else if(unit == 1) {
        queue = writeQueue1;
    }
    else if(unit == 2) {
        queue = writeQueue2;
    }
    else {
        queue = writeQueue3;
    }

    int toWake = MboxCreate(1, 0);

    // put the process in the appropriate queue
    WriteProc currentProc = {
        .pid = getpid(), .buf = buffer, .bufLength = bufSize, 
        .curr = 0, .wakeBox = toWake, .next = NULL
    };

    WriteProc prev = queue;
    while(prev->next != NULL) {
        prev = prev->next;
    }
    prev->next = currentProc;

    // note: number of characters written to terminal is the same as
    // the buffer size. So args->arg2 doesn't change.
    // now put the process to sleep.
    releaseLock(writeLock);
    MboxRecv(toWake, NULL, 0);
}

/*
void termWrite(char* buffer, int bufSize, int unit, int *lenOut) {
	
	// check for invalid input and return if needed
	if (bufSize < 0 || bufSize > MAXLINE || unit < 0 || unit > 3) {
		//args->arg4 = (void*)(long)-1;
		return;
	}
	else {
		//args->arg4 = (void*)(long)0;
	}

	// writing must be done atomically, so we need to grab a lock first
	getLock(writeLock);

	int charactersWritten = 0;
	//args->arg2 = (void*)(long)charactersWritten;

	releaseLock(writeLock);	
}
*/

/* 
 * main function for a daemon that handles putting other processes to sleep
 * and waking them up when their sleep time is up.
 * 
 * Arguments: void pointer arg; unused.
 * Returns: integer representing the status of the daemon, but daemon 
 * should never actually return, so this value can be any integer.
*/
int sleepDaemon(void *arg) {
    
    int currTime = 0;
    while(1) {
        
        // it is a daemon, so it runs an infinite loop
        // wait on clock device -- sleeps until 100 ms later
        waitDevice(USLOSS_CLOCK_DEV, 0, &currTime);
        if(numSleeping > 0 && currTime >= sleepQueue.wakeTime) {
        
            // need to wake up next process in the queue
            numSleeping--;
            int waker = sleepQueue.wakeBox;
            sleepQueue = sleepQueue->next;
        
            // send wakeup message
            MboxSend(waker, NULL, 0);
        }
    }
    return 0; // should never reach this
}

int terminalDaemon(void *arg) {
    	
	int unit = (int)(long)arg;
    int termStatus = 0;
    while(1) {
        			
		waitDevice(USLOSS_TERM_DEV, unit, &termStatus);

        int recvStatus = USLOSS_TERM_STAT_RECV(termStatus);
        int xmitStatus = USLOSS_TERM_STAT_XMIT(termStatus);
        if(recvStatus == USLOSS_DEV_ERROR || xmitStatus == USLOSS_DEV_ERROR) {
            // fatal error. exit as mentioned in the terminal supplement.
            fprintf(stderr, "ERROR: Cannot get data from the terminal.\n");
            USLOSS_Halt(1);
        }

        if (recvStatus == USLOSS_DEV_BUSY /* todo: maybe wrong?? */) {
            // TODO -- add received character to a buffer.
			//termRead(buffer, bufSize, unit, lenOut);
		}
		if (xmitStatus == USLOSS_DEV_READY /* todo: maybe wrong?? */) {
            // TODO TODO TODO
            // if write buffer is empty, wake up waiting process.
            // else, write next character from the write buffer to terminal.
			//termWrite(buffer, bufSize, unit, lenOut);
		}
    }
	return 0; // should never reach this 
}

/*
 * Attempts to grab the lock; blocks and waits if it is unable to.
 * Utility function to grab a specified lock
 * Arguments: integer representing ID of the mailbox serving as our lock
 *   Returns: Void
*/
void getLock(int lock) {
    
    // will block if mailbox is full - prevent concurrency
    MboxSend(lock, NULL, 0);
}

/*
 * Releases the lock; should not be called unless the lock is currently owned.
 * Utility function to release a specified lock.
 * Arguments: integer representing ID of the mailbox serving as a lock.
 *   Returns: Void
*/
void releaseLock(int lock) {
    
    // will block if mailbox is empty, so be careful about when you call this!
    MboxRecv(lock, NULL, 0);
}
