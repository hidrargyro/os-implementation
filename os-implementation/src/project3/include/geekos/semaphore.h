/*
 * Generic list data type
 * Copyright (c) 2011 Jorge Atala <daveho@cs.umd.edu>
 * $Revision: 1.0 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#ifndef GEEKOS_SEMAPHORE_H
#define GEEKOS_SEMAPHORE_H



#define MAX_CHARS_IN_NAME 26
#define MAX_SEMAPHORES 20

struct Semaphore;
unsigned int Semaphores_Counter;

struct Semaphore * semaphores[MAX_SEMAPHORES];


struct Semaphore {
    int id;
    char name[MAX_CHARS_IN_NAME];
    int usable;//cantidad de recursos que puede asignar el semaforo
    struct Kernel_Thread* user_thread;// thread que tiene el bloqueo del semaforo
    struct Thread_Queue wait_Queue;
    struct Thread_Queue references_Queue;//ver si solo guardo los pid
};

int CreateSemaphore(char* name);
int DestroySemaphore(unsigned int id);
int P(unsigned int id);
int V(unsigned int id);
void unbindSemaphore(struct Kernel_Thread* kthread);

#endif
