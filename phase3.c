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
#include <phase3_usermode.h>    /* so trampoline can access syscalls */

/* semaphore struct -- can change later if necessary */
typedef struct Semaphore {
    int count;  /* value of semaphore */
    int blockOn;    /* ID of the mailbox to block on if count = 0 */
    int waiting;    /* indicates num of processes waiting to p on semaphore */
} Semaphore;

typedef struct FuncMessage {
    int(*funcMain)(void*);
    void *arg;
} FuncMessage;

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

Semaphore allocatedSems[MAXSEMS];  /* array of available semaphores */
int filledSems[MAXSEMS];
int allFull = EMPTY;

void phase3_init() {
    // TODO
    // create lock
    lock = MboxCreate(1, 0);
    // initialize filledSems and allocatedSems to 0
    memset(allocatedSems, 0, sizeof(allocatedSems));
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

//int trampolineMain(int(*main)(void*), void* arg) {
int trampolineMain(void* arg) {
    // receive the correct data on which function to call from the mailbox
    int infoBox = (int)(long)arg;
    FuncMessage mainInfo = {.funcMain = NULL, .arg = NULL};
    MboxRecv(infoBox, (void*)&mainInfo, sizeof(FuncMessage));

    int(*funcMain)(void*) = mainInfo.funcMain;
    void *args = mainInfo.arg;

	// switch to user mode
	unsigned int oldPSR = USLOSS_PsrGet();
	if (USLOSS_PsrSet(oldPSR - 1) == USLOSS_ERR_INVALID_PSR) {
		fprintf(stderr, "Bad PSR set in trampolineMain\n");
	}

    // now actually run the main function.
	int mainReturn = (*funcMain)(args);

	// call Terminate to end the process since the process won't end itself
    Terminate(mainReturn);

    // should never reach here (just put it here to placate gcc)
    return mainReturn;
}


void spawn(USLOSS_Sysargs *args) {
    getLock();

    // create mailbox for the process
    int sendFuncID = MboxCreate(1, sizeof(FuncMessage));

    // get the variables related to the process main function
    int(*funcMain)(void*) = (int(*)(void*))args->arg1;
    void *funcArgs = args->arg2;

    // send message
    FuncMessage funcInfo = {.funcMain = funcMain, .arg = funcArgs};
    FuncMessage *functionInfo = &funcInfo;

    MboxSend(sendFuncID, (void*)functionInfo, sizeof(FuncMessage));

    // now use trampoline
    int(*trampoline)(void*) = trampolineMain;

    releaseLock();
    int sporkReturn = spork((char*)args->arg5, trampoline, (void*)(long)sendFuncID, (int)(long)args->arg3, (int)(long)args->arg4);
    getLock();

    //int sporkReturn = spork((char*)args->arg5, (int(*)(void*))args->arg1, args->arg2, (int)(long)args->arg3, (int)(long)args->arg4);

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
    int joinReturn;
    // release lock before calling other function, get it back when returning
    releaseLock();
    joinReturn = join(&joinStatus);
    getLock();

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
        releaseLock();
	    joinReturn = join(&joinStatus);
        getLock();
    }

    int status = (int)(long)args->arg1;

    // just before calling another function, release lock.
    releaseLock();
    quit(status);
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
    allocatedSems[slot].count = (int)(long)(args->arg1);
    // mailbox is only used for blocking, not used to store values
    // this is to save space and only allocate resources we need
    allocatedSems[slot].blockOn = MboxCreate(1, 0);
    allocatedSems[slot].waiting = 0;
    args->arg1 = (void*)(long)slot;
    args->arg4 = (void*)(long)(0);
    releaseLock();
}

void p(USLOSS_Sysargs *args) {
    getLock();
    int slot = (int)(long)args->arg1;
    if(slot < 0 || filledSems[slot] == EMPTY) {
        // invalid arguments!
        args->arg4 = (void*)(long)(-1);
        releaseLock();
        return;
    }
    // arguments passed validity check, now attempt to decrement
    if(allocatedSems[slot].count == 0) {
        // block!
        int mailbox = allocatedSems[slot].blockOn;
        allocatedSems[slot].waiting++;
        releaseLock();
        MboxRecv(mailbox, NULL, 0);
        // after here, we are awake again.
        getLock();
    }
    allocatedSems[slot].count--;
    args->arg4 = (void*)(long)(0);
    releaseLock();
}

void v(USLOSS_Sysargs *args) {
    getLock();
    int slot = (int)(long)(args->arg1);
    if(slot < 0 || filledSems[slot] == EMPTY) {
        args->arg4 = (void*)(long)-1;
        releaseLock();
        return;
    }
    allocatedSems[slot].count++;
    args->arg4 = (void*)(long)0;
    int wakeBox = allocatedSems[slot].blockOn;
    if(allocatedSems[slot].waiting > 0) {
        // only send message if someone waiting
        allocatedSems[slot].waiting--;
        releaseLock();
        MboxSend(wakeBox, NULL, 0);
        return;
    }
    // don't send message if no one waiting, just exit
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
