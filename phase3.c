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

void phase3_init() {
    // TODO
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
    // TODO
}

/* Below: All functions called by the syscall handler. */

void spawn(USLOSS_Sysargs *args) {
    // TODO
}

void wait(USLOSS_Sysargs *args) {
    // TODO
}

void terminate(USLOSS_Sysargs *args) {
    // TODO
}

void semCreate(USLOSS_Sysargs *args) {
    // TODO
}

void p(USLOSS_Sysargs *args) {
    // TODO
}

void v(USLOSS_Sysargs *args) {
    // TODO
}

void getTime(USLOSS_Sysargs *args) {
    // TODO
}

void getPID(USLOSS_Sysargs *args) {
    // TODO
}