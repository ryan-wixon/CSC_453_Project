/*
 * Ryan Wixon and Adrianna Koppes
 * Phase 4a
 * CSC 452-001
 *
 * Incorperates connectivity for device handlers; this currently defines functions which
 * will allow processes to sleep or to read/write with a terminal. It will include disk
 * communication functionality at a later point, but that is unimplemented for now.
 *
 * See the included README file for additional documentation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <usyscall.h>                /* for system call constants, etc. */
#include <phase1.h>                  /* to access phase 1 functions, if necessary */
#include <phase2.h>                  /* to access phase2 functions, for mailboxes */
#include <phase3.h>
#include <phase3_kernelInterfaces.h> /* for semaphore functionality */
#include <phase4.h>

// function prototypes
void phase4_start_service_processes();
void getLock(int lock);
void releaseLock(int lock);
int sleepDaemon(void* arg);
int terminalDaemon(void* arg);
int diskDaemon(void* arg);
void sleep(USLOSS_Sysargs *args);
void termRead(USLOSS_Sysargs *args);
void termWrite(USLOSS_Sysargs *args);
void diskSize(USLOSS_Sysargs *args);
void diskRead(USLOSS_Sysargs *args);
void diskWrite(USLOSS_Sysargs *args);

/* constants for disk processing */
#define TRACKS    0
#define SEEK    1
#define READ    2
#define WRITE    3

/* for sending request infomation to a disk; 1 for each disk */
struct DiskState {
    USLOSS_DeviceRequest req;
} diskRequest[2];

/* for queueing processes for sleeping/waking */
typedef struct SleepProc {
    int pid;          /* ID of the process that is sleeping */
    long wakeTime;    /* time at which the process should wake up (in microseconds) */
    int wakeBox;      /* mailbox to wake the process up */
} SleepProc;

/* for queuing processes for writing to the terminal */
typedef struct WriteProc {
    int pid;                /* ID of the process writing to the terminal */
    char *buf;              /* buffer to write */
    int bufLength;          /* buffer length */
    int curr;               /* index of next char to write */
    int wakeBox;            /* mailbox to wake the process up */
    struct WriteProc *next; /* next process in the queue */
} WriteProc;

/* for queueing components of processes for acessing the disk */
typedef struct DiskProc {
    USLOSS_Sysargs* args;   /* arguments passed in by the current process */
    int pid;                /* ID of the process making requests to the disk */
    int track;              /* track number to request */
    int requestType;        /* int indicating the type of request; 0 = TRACKS, 1 = SEEK, 2 = READ, 3 = WRITE */ 
    int lastStep;           /* boolean indicating if this is the current process's last step */
    int wakeBox;            /* mailbox to wake the process up */
    struct DiskProc *next;  /* next step or process in the queue */
} DiskProc;

int sleepLock = -1;   /* lock for sleep handler - can no longer use global lock */
int readLock = -1;    /* lock for reading from terminal */
int writeLock[4];     /* locks for writing from terminal */
int diskLock[2];      /* locks for accessing disks */

SleepProc sleepQueue[MAXPROC];  /* process table for sleep queue */
int sleepOccupied[MAXPROC];     /* contains whether slots are occupied */
SleepProc *sleepHead;           /* head of the sleep queue */
int numSleeping = 0;            /* number of processes waiting to sleep */

// mailbox IDs for mailboxes used as read queues
int readQueue0;
int readQueue1;
int readQueue2;
int readQueue3;

/* queues for writing to terminals and accessing disks */
WriteProc* writeQueues[4];
DiskProc* diskQueues[2];

/* 
 * Startup code for phase 4; initializes required mailboxes, the shadow process table,
 * and configures interrupts for the terminals.
 * 
 * Arguments: Void
 *   Returns: Void
 */
void phase4_init(void) {
    
    // create locks
    sleepLock = MboxCreate(1, 0);
    readLock = MboxCreate(1, 0);
    for (int i = 0; i < 4; i++) {
        writeLock[i] = MboxCreate(1, 0);
    }
    for (int i = 0; i < 2; i++) {
	diskLock[i] = MboxCreate(1, 0);
    }

    // create read mailboxes (one for each terminal)
    readQueue0 = MboxCreate(10, MAXLINE + 1);
    readQueue1 = MboxCreate(10, MAXLINE + 1);
    readQueue2 = MboxCreate(10, MAXLINE + 1);
    readQueue3 = MboxCreate(10, MAXLINE + 1);

    for (int i = 0; i < 4; i++) {
        writeQueues[i] = NULL;
    }
    for (int i = 0; i < 2; i++) {
	diskQueues[i] = NULL;
    }

    // unmask terminal interrupts
    int control = 0x0;  // don't send a character
    control |= 0x2;     // enable read interrupts
    control |= 0x4;     // enable write interrupts

    // now unmask interrupts for each terminal
    int result0 = USLOSS_DeviceOutput(USLOSS_TERM_DEV, 0, (void*)(long)control);
    int result1 = USLOSS_DeviceOutput(USLOSS_TERM_DEV, 1, (void*)(long)control);
    int result2 = USLOSS_DeviceOutput(USLOSS_TERM_DEV, 2, (void*)(long)control);
    int result3 = USLOSS_DeviceOutput(USLOSS_TERM_DEV, 3, (void*)(long)control);
    
    if (result0 == USLOSS_DEV_INVALID || result1 == USLOSS_DEV_INVALID || 
            result2 == USLOSS_DEV_INVALID || result3 == USLOSS_DEV_INVALID) {
        fprintf(stderr, "ERROR: Failed to unmask interrupts in the terminals.\n");
        USLOSS_Halt(1);
    }

    // null out the process tables and set occupancy tables to vacant
    memset(sleepQueue, 0, sizeof(sleepQueue));
    for(int i = 0; i < MAXPROC; i++) {
        sleepOccupied[i] = 0;
    }

    // register the syscalls
    systemCallVec[SYS_SLEEP] = sleep;
    systemCallVec[SYS_TERMREAD] = termRead;
    systemCallVec[SYS_TERMWRITE] = termWrite;
    systemCallVec[SYS_DISKSIZE] = diskSize;
    systemCallVec[SYS_DISKREAD] = diskRead;
    systemCallVec[SYS_DISKWRITE] = diskWrite;
}

/* 
 * Creates helper processes which manage the requests of user code. This includes a
 * daemon process sleeping as well as 4 daemon processes managing each terminal.
 * 
 * Arguments: None
 *   Returns: Void
 */
void phase4_start_service_processes() {

    // start daemons -- will all have max priority.
    int (*sleepFunc)(void*) = sleepDaemon;
    int (*termFunc0)(void*) = terminalDaemon;
    int (*termFunc1)(void*) = terminalDaemon;
    int (*termFunc2)(void*) = terminalDaemon;
    int (*termFunc3)(void*) = terminalDaemon;
    int (*diskFunc0)(void*) = diskDaemon;
    int (*diskFunc1)(void*) = diskDaemon;
    spork("sleepDaemon", sleepFunc, (void*)(long)0, USLOSS_MIN_STACK, 1);
    spork("termDaemon0", termFunc0, (void*)(long)0, USLOSS_MIN_STACK, 1);
    spork("termDaemon1", termFunc1, (void*)(long)1, USLOSS_MIN_STACK, 1);
    spork("termDaemon2", termFunc2, (void*)(long)2, USLOSS_MIN_STACK, 1);
    spork("termDaemon3", termFunc3, (void*)(long)3, USLOSS_MIN_STACK, 1);
    spork("diskDaemon0", diskFunc0, (void*)(long)0, USLOSS_MIN_STACK, 1);
    spork("diskDaemon1", diskFunc1, (void*)(long)1, USLOSS_MIN_STACK, 1);
}

/* Phase 4a system call handlers */

/* 
 * Attempts to put a process to sleep based on a provide time in seconds. The process
 * will be awakened prompty after it's alloted time has passed, although matching it
 * exactly is not guaranteed. In other words, it will never sleep for less than the given
 * time, but it could sleep for more.
 * 
 * Arguments: USLOSS_Sysargs struct containing the arguments to this syscall
 *   Returns: Void
 */
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
	
    // add new process to the queue
    int index = 0;
    for(int i = 0; i < MAXPROC; i++) {
        if(sleepOccupied[i] == 0) {
            sleepOccupied[i] = 1;
            sleepQueue[i].pid = getpid();
            sleepQueue[i].wakeBox = MboxCreate(1,0);
            // recall: USLOSS clock gives microseconds!!
            sleepQueue[i].wakeTime = currentTime() + (waitTime * 1000000);
            index = i;
            break;
        }
    }

    // prepare args for return.
    args->arg4 = (void*)(long)0;

    int wakeBoxTemp = sleepQueue[index].wakeBox;
    
    // put the process to sleep
    releaseLock(sleepLock);
    MboxRecv(wakeBoxTemp, NULL, 0);

    // control returns to the current process -- it can now continue.
}

/* 
 * Attempts to read from a terminal by adding the current process to a read queue.
 * The actual read will not take place until the terminal daemon receives an
 * interrupt.
 * 
 * Arguments: USLOSS_Sysargs struct containing the arguments to this syscall
 *   Returns: Void
 */
void termRead(USLOSS_Sysargs *args) {
    
    getLock(readLock);
    
    char* buffer = (char*)args->arg1;   // possibly need to fix syntax here
    int bufSize = (int)(long)args->arg2;
    int unit = (int)(long)args->arg3;
    
    // check for invalid input and return if needed
    if (bufSize <= 0 || bufSize > MAXLINE || unit < 0 || unit > 3) {
	    args->arg4 = (void*)(long)-1;
        releaseLock(readLock);
	    return;
    }
    else {
	    args->arg4 = (void*)(long)0;
    }
	
    // figure out which terminal to read to
    int readQueue =  -1;
    if (unit == 0) {
        readQueue = readQueue0;
    }
    else if (unit == 1) {
        readQueue = readQueue1;
    }
    else if (unit == 2) {
        readQueue = readQueue2;
    }
    else {
        // we already filtered out invalid terminal units
        readQueue = readQueue3;
    }

    // receive from the read mailbox.
    int size = 0;
    char tempBuf[MAXLINE];
    memset(tempBuf, 0, sizeof(tempBuf));
    releaseLock(readLock);
    size = MboxRecv(readQueue, tempBuf, MAXLINE);
    if(size < bufSize) {
        // couldn't read in as many characters as we wanted
        bufSize = size;
    }
    memcpy(buffer, tempBuf, bufSize * sizeof(char));

    args->arg2 = (void*)(long)bufSize;
    return;
}

/* 
 * Attempts to write to a terminal by adding the current process to a write queue.
 * The actual write will not take place until the terminal daemon receives an
 * interrupt.
 * 
 * Arguments: USLOSS_Sysargs struct containing the arguments to this syscall
 *   Returns: Void
 */
void termWrite(USLOSS_Sysargs *args) {
    	
    int unit = (int)(long)args->arg3;

    getLock(writeLock[unit]);
    char *buffer = (char*)args->arg1;
    int bufSize = (int)(long)args->arg2;
    
    // check for invalid input and return if needed
    if (bufSize < 0 || bufSize > MAXLINE || unit < 0 || unit > 3) {
	args->arg4 = (void*)(long)-1;
    	releaseLock(writeLock[unit]);
	return;
    }
    else {
	args->arg4 = (void*)(long)0;
    }

    int toWake = MboxCreate(1, 0);

    // put the process in the appropriate queue
    WriteProc currentProc = {
        .pid = getpid(), .buf = buffer, .bufLength = bufSize, 
        .curr = 0, .wakeBox = toWake, .next = NULL
    };

    if (writeQueues[unit] == NULL) {
	writeQueues[unit] = &currentProc;
    }
    else {
        WriteProc* prev = writeQueues[unit];
    	while (prev->next != NULL) {
            prev = prev->next;
    	}
    	prev->next = &currentProc;
    }

    // note: number of characters written to terminal is the same as
    // the buffer size. So args->arg2 doesn't change.

    // now put the process to sleep.
    releaseLock(writeLock[unit]);
    MboxRecv(toWake, NULL, 0);
}

void diskSize(USLOSS_Sysargs* args) {

	int unit = (int)(long)args->arg1;
	if (unit < 0 || unit > 1) {
		printf("ERROR: Invalid disk number given to diskSize.\n");
		args->arg4 = (void*)(long)-1;
	}
	args->arg4 = (void*)(long)0;

	// grab the lock for this disk
	getLock(diskLock[unit]);

    int toWake = MboxCreate(1, 0);

	DiskProc currentProc = {
		.args = args, .pid = getpid(), .requestType = 0, 
		.lastStep = 1, .wakeBox = toWake, .next = NULL
	};

	// add in a new TRACK step to the disk's queue
	DiskProc* curr = diskQueue[unit];
	if (curr == NULL) {
		curr = &currentProc;
	}
	else {
		while (curr->next != NULL) {
			curr = curr->next;
		}
		curr->next = &currentProc;
	}

	// now put the process to sleep
	releaseLock(diskLock[unit]);
	MboxRecv(toWake, NULL, 0);
}

void diskRead(USLOSS_Sysargs* args) {

	int unit = (int)(long)args->arg1;
	if (unit < 0 || unit > 1) {
		printf("ERROR: Invalid disk number given to diskRead.\n");
		args->arg4 = (void*)(long)-1;
	}
	args->arg4 = (void*)(long)0;

	getLock(diskLock[unit]);

    // unpack the sysargs struct
    int unit = (int)(long)args->arg5;
    int track = (int)(long)args->arg3;
    int startBlock = (int)(long)args->arg4;
    int sectors = (int)(long)args->arg2;

    DiskProc* curr = diskQueues[unit];
    if(curr != NULL) {
        while(curr->next != NULL && !(curr->next.lastStep == 1 && (int)(long)curr->next.track >= track)) {
            // find the next place where the process can go
            curr = curr->next;
        }
    }

    // now add the processes to the disk queue
    int toWake = MboxCreate(1, 0);

    // first find next place to go
    if(curr == NULL) {
        // first add seek operation to get to the right track
        DiskProc seekCurr = {
		    .args = args, .pid = getpid(), .track = track, 
            .requestType = 1, .lastStep = 0, .wakeBox = toWake, 
            .next = NULL
	    };
        diskQueues[unit] = &seekCurr;
    }
    else if((int)(long)curr->args->args3 < track) {
        // first add a seek operation to get to the right track
        DiskProc seekCurr = {
		    .args = args, .pid = getpid(), .track = track, 
            .requestType = 1, .lastStep = 0, .wakeBox = toWake, 
            .next = NULL
	    };
        seekCurr->next = curr->next;
        curr->next = seekCurr;
    }
    else if(curr->next == NULL) {
        // start elevator back at the bottom
        DiskProc seekZero = {
		    .args = args, .pid = getpid(), .track = 0, 
            .requestType = 1, .lastStep = 0, .wakeBox = toWake, 
            .next = NULL
	    };

        DiskProc seekCurr = {
		    .args = args, .pid = getpid(), .track = track, 
            .requestType = 1, .lastStep = 0, .wakeBox = toWake, 
            .next = NULL
	    };

        seekZero->next = seekCurr;
        seekCurr->next = curr->next;
        curr->next = seekZero;
    }
    // if we are already at the correct track, do nothing

    // save next pointer of curr for later
    DiskProc *temp = curr->next;

    // add new queue operation for every block to operate on
    int blocksLeft = sectors;
    int blockToAdd = startBlock;
    
    while(blocksLeft > 0) {
        if(blockToAdd > 15) {
            // TODO add a seek operation to the next track up
            blockToAdd = -1; // reset block to add so incrementing will give 0
        }
        // TODO add read operation
        blocksLeft--;
        blockToAdd++;
    }

	// TODO add the required SEEK and READ operations into the queue. The blocks
	// don't need to be read in order; the head must seek forward to the nearest
	// one and wrap to the front of the disk once it reaches the end. Using the
	// starting point we can figure out what order the blocks should be read, and
	// add them into the queue in the correct order, this will make that daemon's
	// job very easy.

	releaseLock(diskLock[unit]);
    MboxRecv(toWake, NULL, 0);
}

void diskWrite(USLOSS_Sysargs* args) {
	// TODO See above comment

	int unit = (int)(long)args->arg1;
	if (unit < 0 || unit > 1) {
		printf("ERROR: Invalid disk number given to diskWrite.\n");
		args->arg4 = (void*)(long)-1;
	}
	args->arg4 = (void*)(long)0;

	getLock(diskLock[unit]);

    // unpack the sysargs struct
    int unit = (int)(long)args->arg5;
    int track = (int)(long)args->arg3;
    int startBlock = (int)(long)args->arg4;
    int sectors = (int)(long)args->arg2;

    DiskProc* curr = diskQueues[unit];
    if(curr != NULL) {
        while(curr->next != NULL && !(curr->next.lastStep == 1 && (int)(long)curr->next.track >= track)) {
            // find the next place where the process can go
            curr = curr->next;
        }
    }

    // now add the processes to the disk queue
    int toWake = MboxCreate(1, 0);

    // first find next place to go
    if(curr == NULL) {
        // first add seek operation to get to the right track
        DiskProc seekCurr = {
		    .args = args, .pid = getpid(), .track = track, 
            .requestType = 1, .lastStep = 0, .wakeBox = toWake, 
            .next = NULL
	    };
        diskQueues[unit] = &seekCurr;
    }
    else if((int)(long)curr->args->args3 < track) {
        // first add a seek operation to get to the right track
        DiskProc seekCurr = {
		    .args = args, .pid = getpid(), .track = track, 
            .requestType = 1, .lastStep = 0, .wakeBox = toWake, 
            .next = NULL
	    };
        seekCurr->next = curr->next;
        curr->next = seekCurr;
    }
    else if(curr->next == NULL) {
        // start elevator back at the bottom
        DiskProc seekZero = {
		    .args = args, .pid = getpid(), .track = 0, 
            .requestType = 1, .lastStep = 0, .wakeBox = toWake, 
            .next = NULL
	    };

        DiskProc seekCurr = {
		    .args = args, .pid = getpid(), .track = track, 
            .requestType = 1, .lastStep = 0, .wakeBox = toWake, 
            .next = NULL
	    };

        seekZero->next = seekCurr;
        seekCurr->next = curr->next;
        curr->next = seekZero;
    }
    // if we are already at the correct track, do nothing

    // save next pointer of curr for later
    DiskProc *temp = curr->next;

    // add new queue operation for every block to operate on
    int blocksLeft = sectors;
    int blockToAdd = startBlock;
    
    while(blocksLeft > 0) {
        if(blockToAdd > 15) {
            // TODO add a seek operation to the next track up
            blockToAdd = -1; // reset block to add so incrementing will give 0
        }
        // TODO add write operation
        blocksLeft--;
        blockToAdd++;
    }
	
	// TODO add the required SEEK and WRITE operations into the queue. This will 
	// work the same way as the read syscall, just with writing operations instead.

	releaseLock(diskLock[unit]);
    MboxRecv(toWake, NULL, 0);
}

/* 
 * Main function for a daemon that handles putting other processes to sleep
 * and waking them up when their sleep time is up.
 * 
 * Arguments: void pointer arg; unused.
 *   Returns: 0, but daemon should never actually return, so this value can be any integer.
 */
int sleepDaemon(void *arg) {
    
    int currTime = 0;
    while (1) {
        
        // it is a daemon, so it runs an infinite loop
        // wait on clock device -- sleeps until 100 ms later
        waitDevice(USLOSS_CLOCK_DEV, 0, &currTime);

        if(numSleeping > 0) {
        
            // need to wake up next process in the queue
            for(int i = 0; i < MAXPROC; i++) {
                if(sleepOccupied[i] == 1 && currTime > sleepQueue[i].wakeTime) {
                    numSleeping--;
                    int waker = sleepQueue[i].wakeBox;
                    sleepOccupied[i] = 0;
                    MboxSend(waker, NULL, 0);
                }
            }
        }
    }
    return 0; // should never reach this
}

/* 
 * Main function for a daemon that handles communications with a terminal. This daemon will
 * only manage 1 specific terminal; when it receives an interrupt it will perform a read, write
 * both, or do nothing.
 * 
 * Arguments: void pointer arg containing an int from 0-3 representing the unit number
 *   Returns: 0, but daemon should never actually return, so this value can be any integer.
 */
int terminalDaemon(void *arg) {
    	
    int unit = (int)(long)arg;
    int termStatus = 0;

    int readQueue = -1;

    // figure out which terminal to set up for reading
    if (unit == 0) {
        readQueue = readQueue0;
    }
    else if (unit == 1) {
        readQueue = readQueue1;
    }
    else if (unit == 2) {
        readQueue = readQueue2;
    }
    else {
        // this is guaranteed to be 3
        readQueue = readQueue3;
    }

    // create a buffer to hold characters being read from the terminal
    char readBuffer[MAXLINE];
    int curr = 0;
    memset(readBuffer, 0, MAXLINE);

    while(1) {
        			
	waitDevice(USLOSS_TERM_DEV, unit, &termStatus);	
        WriteProc* writeQueue = writeQueues[unit];

        int recvStatus = USLOSS_TERM_STAT_RECV(termStatus);
        int xmitStatus = USLOSS_TERM_STAT_XMIT(termStatus);
        if(recvStatus == USLOSS_DEV_ERROR || xmitStatus == USLOSS_DEV_ERROR) {
            
	    // fatal error; exit as mentioned in the terminal supplement
            fprintf(stderr, "ERROR: Cannot get data from the terminal.\n");
            USLOSS_Halt(1);
        }

        if (recvStatus == USLOSS_DEV_BUSY) {
            // add received character to a buffer
            // if the buffer is full, send message to associated mailbox
            // if there is a waiting process, this will wake up that process without us having to do anything

	    // read the character in
	    readBuffer[curr] = USLOSS_TERM_STAT_CHAR(termStatus);
	    curr++;

	    // if the character was a newline, or the buffer is full, send to the mailbox and reset the buffer
	    if (readBuffer[curr - 1] == '\n' || curr == MAXLINE) {
	    	MboxCondSend(readQueue, readBuffer, curr);
		curr = 0;
		memset(readBuffer, 0, MAXLINE);
	    }		
	}
	if (writeQueue != NULL && xmitStatus == USLOSS_DEV_READY) {
        getLock(writeLock[unit]);

	    // create the integer value that will be sent to to the terminal
	    int sendValue = 0x7;
	    sendValue |= (writeQueue->buf[writeQueue->curr] << 8);
	    
	    // send the value
	    if (USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long)sendValue) == USLOSS_DEV_INVALID) {
		fprintf(stderr, "ERROR: Invalid parameters passed to USLOSS_DeviceOutput\n");
        	releaseLock(writeLock[unit]);
        	USLOSS_Halt(1);
	    }
	    writeQueue->curr++;


	    // if the end of the string has been reached, wake up the waiting process and remove it from the queue
	    if (writeQueue->curr == writeQueue->bufLength) {
		WriteProc* finishedProcess = writeQueue;
		writeQueues[unit] = writeQueues[unit]->next;
		MboxSend(finishedProcess->wakeBox, NULL, 0);
	    }  
            releaseLock(writeLock[unit]);  
	}
    }
    return 0; // should never reach this 
}

int diskDaemon(void* arg) {

	int unit = (int)(long)arg;
	int diskStatus = 0;

	while (1) {
		waitDevice(USLOSS_DISK_DEV, unit, &diskStatus);

        if(diskStatus == USLOSS_DEV_ERROR) {
            // encountered an error -- deliver the status to the waiting process
            DiskQueue* nextStep = diskQueues[unit];
            nextStep->args->arg1 = (void*)(long)diskStatus;
            int waker = diskQueues[unit]->wakeBox;
            // advance the queue to get rid of everything related to the current
            // attempt to read
            while(diskQueues[unit] != NULL && diskQueues[unit]->lastStep != 1) {
                diskQueues[unit] = diskQueues[unit]->next;
            }
            if(diskQueues[unit] != NULL) {
                // advance the pointer by one more to get past the last step
                diskQueues[unit] = diskQueues[unit]->next;
            }
            // wake up the waiting process
            MboxSend(waker, NULL, 0);
        }
		if(diskStatus == USLOSS_DEV_READY) {
            // we are ready to work with the disk

			DiskQueue* nextStep = diskQueues[unit];
            if(nextStep == NULL) {
                // nothing in the queue
                continue;
            }
			if (nextStep->requestType == 0) {
				nextStep->args->arg1 = 512;
				nextStep->args->arg2 = 16;
				diskRequest[unit].req.op = USLOSS_DISK_TRACKS;
				diskRequest[unit].req.reg1 = nextStep->args->arg3;
				USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, diskRequest[unit]);
			}
			else if (nextStep->requestType == 1) {
				// code for seeking a block
			}
			else if (nextStep->requestType == 2) {
				// code for reading a block
			}
			else {
				// code for writing a block
			}
	
			// is this the last step of the operation?
			if (nextStep->lastStep == 1) {
                // wake up the waiting process
                int waker = nextStep.wakeBox;
                MboxSend(waker, NULL, 0);
				continue;
			}
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
