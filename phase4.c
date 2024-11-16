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
#include <phase1.h>  /* to access phase 1 functions, if necessary */
#include <phase2.h>  /* to access phase2 functions, for mailboxes */
#include <phase3.h>
#include <phase3_kernelInterfaces.h> /* for semaphore functionality */
//#include <phase3_usermode.h>
#include <phase4.h>

void phase4_start_service_processes();

/* for queueing processes for sleeping/waking */
typedef struct SleepProc {
    int pid;    /* ID of the process that is sleeping */
    long wakeTime;   /* time at which the process should wake up (in microseconds) */
    int wakeBox;   /* ID of the mailbox used to wake the process */
    SleepProc *next;  /* next process in the queue */
} SleepProc;

int sleepLock = -1;    /* lock for Sleep handler - can no longer use global lock */
int readLock = -1;  /* lock for reading from terminal */
int writeLock = -1;   /* lock for writing to terminal */
// Phase 4b -- add locks for the other interrupts

SleepProc sleepQueue = NULL; /* head of the sleep queue */
int numSleeping = 0;

void phase4_init(void) {
    // create locks
    sleepLock = MboxCreate(1, 0);
    readLock = MboxCreate(1, 0);
    writeLock = MboxCreate(1, 0);
    // TODO
}

void phase4_start_service_processes() {
    // TODO -- start daemons
    //   - clock daemon (to call waitDevice)
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
    if(waitTime < 0) {  // TODO -- is wait time = 0 valid or invalid? need Russ response
        // illegal wait time!
        args->arg4 = (void*)(long)-1;
        releaseLock(sleepLock);
        return;
    }
    // valid wait time
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
    // TODO
}

void termWrite(USLOSS_Sysargs *args) {
    // TODO
}

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
    return 0;  // should never reach this.
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