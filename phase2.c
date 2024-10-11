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

int send(int mboxID, void *message, int msgSize, int doesBlock);
int receive(int mboxID, void *message, int maxMsgSize, int doesBlock);
void phase2_start_service_processes();
void nullsys(void);
// TODO ADD INTERRUPT HANDLERS AS DESCRIBED IN SPEC

// TODO ADD THE OTHER REQUIRED DATA STRUCTURES
// vector of system calls - set every single element to nullsys (see below)
void (*systemCallVec[])(USLOSS_Sysargs *args);

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

    // TODO - implement phase2_init

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
    // TODO
    return -1;  // dummy return value! replace!!
}

int MboxRelease(int mbox_id) {
    // TODO
    return -1;   // dummy return value! replace!!
}

int MboxSend(int mbox_id, void *msg_ptr, int msg_size) {
    // TODO
    return -1; // dummy return value! replace!!
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