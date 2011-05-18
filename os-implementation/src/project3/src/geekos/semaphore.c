/*
 * Generic list data type
 * Copyright (c) 2011 Jorge Atala <daveho@cs.umd.edu>
 * $Revision: 1.0 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */
#include <geekos/list.h>
#include <geekos/screen.h>
#include <geekos/kthread.h>
#include <geekos/int.h>
#include <geekos/semaphore.h>
#include <string.h>
#include <geekos/malloc.h>
struct Semaphore;
unsigned int Semaphores_Counter=0;

struct Semaphore * semaphores[MAX_SEMAPHORES];


//void Init_Semaphores(void);
int CreateSemaphore(char *name)
{
    int i, first_free_position=-1;
    for(i=0;i<MAX_SEMAPHORES;++i) { 
        //guardo la primera posicion vacia en el array de semaforos
        if(semaphores[i]==NULL && first_free_position==-1)first_free_position=i;
        if(semaphores[i]!=NULL && (strcmp(semaphores[i]->name,name)==0))
            Add_To_Back_Of_Thread_Queue(&(semaphores[first_free_position]->references_Queue),Get_Current());
            if(semaphores[i]->usable==0) {
                Add_To_Back_Of_Thread_Queue(&(semaphores[first_free_position]->wait_Queue),Get_Current());
                Wait(&semaphores[i]->wait_Queue);
            }
            return semaphores[i]->id;
    }
    if(Semaphores_Counter< MAX_SEMAPHORES) {
        ++Semaphores_Counter;
        if(first_free_position==-1)first_free_position= Semaphores_Counter;//posicion del array donde va aquedar el nuevo semaforo

        //le asigno memoria
        semaphores[first_free_position]=(struct Semaphore *)Malloc(sizeof(struct Semaphore));
        if(semaphores[first_free_position]==NULL)return -1;

        //cargo datos iniciales
        semaphores[first_free_position]->id=first_free_position;
        semaphores[first_free_position]->usable=1;
        strcpy(name,semaphores[first_free_position]->name);
        Add_To_Back_Of_Thread_Queue(&(semaphores[first_free_position]->references_Queue),Get_Current());
        return semaphores[first_free_position]->id;
    }
    else //no se pueden crear mas semaforos
        return -1;
}


int DestroySemaphore(unsigned int id)
{
    
    int i;
    for(i=0;i<MAX_SEMAPHORES;++i) { 
        if(semaphores[i]!=NULL && semaphores[i]->id==id) {
            if(Is_Member_Of_Thread_Queue(&semaphores[i]->references_Queue,Get_Current())) {//si este proceso tiene referencia al semaforo lo puede destruir, sino no
                Remove_From_Thread_Queue(&(semaphores[i]->references_Queue),Get_Current()); 
                //si no hay mas referencias al semaforo lo destruyo
                //Disable_Interrupts();
                if(Get_Front_Of_Thread_Queue(&(semaphores[i]->references_Queue))==NULL) {
                    Clear_Thread_Queue(&semaphores[i]->references_Queue);
                    Clear_Thread_Queue(&semaphores[i]->wait_Queue);
                    Free((void * )semaphores[i]);
                    semaphores[i]=NULL;
                    --Semaphores_Counter;
                }
                else {
                    if(Get_Current()->pid==(semaphores[i]->user_thread)->pid) {
                        --semaphores[i]->usable;
                        semaphores[i]->user_thread=NULL;
                        Wake_Up_One(&semaphores[i]->wait_Queue);
                    }
                }    
                //Enable_Interrupts();
                return 0;
            }
        }
    }
    
    return -1;
}


int P(unsigned int id)
{
    int i;
    for(i=0;i<MAX_SEMAPHORES;++i) { 
        if(semaphores[i]!=NULL && semaphores[i]->id==id) {
            if(Is_Member_Of_Thread_Queue(&semaphores[i]->references_Queue,Get_Current())) {
                //Disable_Interrupts();
                if(semaphores[i]->usable>0) {
                    --semaphores[i]->usable;
                    semaphores[i]->user_thread=Get_Current();
                    return 0;
                }
                else //Enable_Interrupts();
                    if((semaphores[i]->user_thread)->pid != Get_Current()->pid)
                        Wait(&semaphores[i]->wait_Queue);//Si llega hasta aca, es porque el semaforo esta ocupado
            }
         }
    }
    
    return -1; 
}
int V(unsigned int id)
{
    int i;
    for(i=0;i<MAX_SEMAPHORES;++i) { 
        if(semaphores[i]!=NULL && semaphores[i]->id==id) {
            if((semaphores[i]->user_thread)->pid == Get_Current()->pid) {
                //Disable_Interrupts();
                ++semaphores[i]->usable;
                semaphores[i]->user_thread=NULL;
                Wake_Up_One(&semaphores[i]->wait_Queue);
                //Enable_Interrupts();
                return 0;
            }
        }
    }
    return -1;
}

void unbindSemaphore(struct Kernel_Thread* unbinded)
{
    int i;
    for(i=0;i<MAX_SEMAPHORES;++i) { 
        if(semaphores[i]!=NULL && Is_Member_Of_Thread_Queue(&semaphores[i]->references_Queue,unbinded)) {
            Remove_From_Thread_Queue(&(semaphores[i]->references_Queue),unbinded);//lo saco de la lista de referencias del semaforo
            if((semaphores[i]->user_thread)->pid == unbinded->pid) {
                //si era el que bloqueaba el semaforo, lo desbloqueo y levanto otro
                ++semaphores[i]->usable;
                semaphores[i]->user_thread=NULL;
                Wake_Up_One(&semaphores[i]->wait_Queue);
            }
            else //Si no era el que bloqueaba, estaba en espera, lo saco de esa lista
                Remove_From_Thread_Queue(&(semaphores[i]->wait_Queue),unbinded);
        }
    }
}
