/*
 *  Name: Shlomo Rabinovich
 *  ID:   308432517
 */

#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <sys/types.h>
#include "osqueue.h"

typedef enum {RUNNING, FINISH, TERMINATE} status;

typedef struct thread_pool
{
    pthread_t * threads;
    pthread_mutex_t mutex;
    pthread_cond_t cv;
    OSQueue *queue;
    int numOfThreads;
    status status;
}ThreadPool;


typedef struct task
{
    void (*function)(void*);
    void* params;
} Task;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

void error();

#endif
