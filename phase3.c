/*
 * Ryan Wixon and Adrianna Koppes
 * CSC 452
 * phase3.h
 * 
 * documentation TODO
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <usyscall.h> /* for system call constants, etc. */
#include <phase1.h>  /* to access phase 1 functions, if necessary */
#include <phase2.h>  /* to access phase2 functions, for mailboxes */
#include <phase3.h>

/* semaphore struct -- can change later if necessary */
typedef struct Semaphore {
    int count;  /* value of semaphore */
    int blockOn;    /* ID of the mailbox to block on if count = 0 */
} Semaphore;

/* function declarations */
void phase3_start_service_processes();
void spawn(USLOSS_Sysargs *args);
void wait(USLOSS_Sysargs *args);
void terminate(USLOSS_Sysargs *args);
void semCreate(USLOSS_Sysargs *args);
void p(USLOSS_Sysargs *args);
void v(USLOSS_Sysargs *args);
void getTime(USLOSS_Sysargs *args);
void getPID(USLOSS_Sysargs *args);

/* mailbox lock functions */
void getLock();
void releaseLock();


/* semephore helper functions */
int nextAvailableSem();

int lock = -1;  /* the ID of the mailbox used as a lock */

#define EMPTY         0
#define FULL         1

Semaphore semaphores[MAXSEMS];  /* array of available semaphores */
int filledSems[MAXSEMS];
int allFull = EMPTY;

void phase3_init() {
    // TODO
    // create lock
    lock = MboxCreate(1, 0);
    // initialize filledSems and semaphores to 0
    memset(semaphores, 0, sizeof(semaphores));
    memset(filledSems, 0, sizeof(filledSems));
    // install the proper functions in the syscallVec (from phase 2)
    systemCallVec[SYS_SPAWN] = spawn;
    systemCallVec[SYS_WAIT] = wait;
    systemCallVec[SYS_TERMINATE] = terminate;
    systemCallVec[SYS_SEMCREATE] = semCreate;
    systemCallVec[SYS_SEMP] = p;
    systemCallVec[SYS_SEMV] = v;
    // note: we are not implementing semFree this semester.
    systemCallVec[SYS_GETTIMEOFDAY] = getTime;
    systemCallVec[SYS_GETPID] = getPID;
    // POSSIBLY TODO MORE??
}

void phase3_start_service_processes() {
    // TODO - this will be do nothing if we don't have service processes
}

/* Below: All functions called by the syscall handler. */

void spawn(USLOSS_Sysargs *args) {
    getLock();

    int sporkReturn = spork((char*)args->arg5, (int(*)(void*))args->arg1, args->arg2, (int)(long)args->arg3, (int)(long)args->arg4);

    if (sporkReturn > 0) {
	args->arg1 = (void*)(long)sporkReturn;
    }
    else {
	args->arg1 = (void*)(long)-1;
    }

    // TODO not sure how to set this yet, leaving as 0 for now
    args->arg4 = 0;

    // TODO
    // note: the only time you can modify PSR is in the trampoline function
    // for the user-main function, which is not yet implemented.
    // TODO -- create and implement user-mode trampoline main function!
    releaseLock();
}

void wait(USLOSS_Sysargs *args) {
    getLock();
   
    int joinStatus;
    int joinReturn = join(&joinStatus);

    if (joinReturn > 0) {
	args->arg1 = (void*)(long)joinReturn;
	args->arg2 = (void*)(long)joinStatus;
	args->arg4 = (void*)(long)0;
    }
    else {
    	args->arg4 = (void*)(long)-2;
    }

    releaseLock();
}

void terminate(USLOSS_Sysargs *args) {
    getLock();
    
    int joinStatus;
    int joinReturn = -1;
    while (joinReturn != -2) {
	joinReturn = join(&joinStatus);
    }

    quit((int)(long)args->arg1);
    
    releaseLock();
}

void semCreate(USLOSS_Sysargs *args) {
    getLock();
    int start = (int)(long)(args->arg1);
    if(start < 0) {
        // bad arguments, return error code
        args->arg4 = (void*)(long)(-1);
        releaseLock();
        return;
    }
    else if(allFull == FULL) {
        // no space remaining
        args->arg4 = (void*)(long)(-1);
        releaseLock();
        return;
    }
    int slot = nextAvailableSem();
    if(slot == -1) {
        // found out that no space remaining
        args->arg4 = (void*)(long)(-1);
        releaseLock();
        return;
    }
    filledSems[slot] = FULL;
    // keep track of the semaphore's value here
    semaphores[slot].count = (int)(long)(args->arg1);
    // mailbox is only used for blocking, not used to store values
    // this is to save space and only allocate resources we need
    semaphores[slot].blockOn = MboxCreate(1, 0);
    args->arg1 = (void*)(long)slot;
    args->arg4 = (void*)(long)(0);
    releaseLock();
}

void p(USLOSS_Sysargs *args) {
    getLock();
    int slot = (int)(long)(args->arg1);
    if(slot < 0 || filledSems[slot] == EMPTY) {
        // invalid arguments!
        args->arg4 = (void*)(long)(-1);
        releaseLock();
        return;
    }
    // arguments passed validity check, now attempt to decrement
    if(semaphores[slot].count == 0) {
        // block!
        int mailbox = semaphores[slot].blockOn;
        releaseLock();
        MboxRecv(mailbox, NULL, 0);
        // after here, we are awake again.
        getLock();
    }
    semaphores[slot].count--;
    args->arg4 = (void*)(long)(0);
    releaseLock();
}

void v(USLOSS_Sysargs *args) {
    getLock();
    int slot = (int)(long)(args->arg1);
    semaphores[slot].count++;
    // conditionally send message to unblock potentially blocked
    // processes (conditional since we have no way to check if
    // any processes currently blocked)
    MboxCondSend(slot, NULL, 0);
    // TODO
    releaseLock();
}

void getTime(USLOSS_Sysargs *args) {
    getLock();
    int time = currentTime();
    args->arg1 = (void*)(long)time;
    releaseLock();
}

void getPID(USLOSS_Sysargs *args) {
    getLock();
    int procPID = getpid();
    args->arg1 = (void*)(long)procPID;
    releaseLock();
}

void getLock() {
    // will block if mailbox is full - prevent concurrency
    MboxSend(lock, NULL, 0);
}

void releaseLock() {
    // will block if mailbox is empty, so be careful about when you call this!
    MboxRecv(lock, NULL, 0);
}

int nextAvailableSem() {
    // don't need to lock; we already have the lock
    for(int i = 0; i < MAXSEMS; i++) {
        if(filledSems[i] == EMPTY) {
            // found an unallocated semaphore
            return i;
        }
    }
    // no available semaphore spaces
    allFull = FULL;
    return -1;
}
