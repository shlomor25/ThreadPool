//
// Created by shlomo on 04/05/19.
//
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <pthread.h>
#include "threadPool.h"
#include "osqueue.h"

#define STDERR 2
#define EXIT -1

static void* manage(void* param);

/**
 * error in system call.
 *
 * write message to stderr
 * then, exit
 */
void error(){
    char* msg = "ERROR in System Call\n";
    write(STDERR, msg, strlen(msg));
    exit(EXIT);
}

/**
 * create Thread Pool.
 *
 * @param numOfThreads in pool
 * @return
 */
ThreadPool* tpCreate(int numOfThreads){
    int i;
    // invalid input
    if (numOfThreads <= 0) return 0;
    // create thread pool
    ThreadPool* tp = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (tp == NULL) error();
    tp->queue = osCreateQueue();
    tp->numOfThreads = numOfThreads;
    tp->status = RUNNING;
    // todo PTHREAD_MUTEX_ERRORCHECK
    if (0 != pthread_mutex_init(&tp->mutex, NULL)) error();
    if (0 != pthread_cond_init(&tp->cv, NULL)) error();
    tp->threads = (pthread_t*)malloc(sizeof(pthread_t)*(size_t)numOfThreads);
    if (tp->threads == NULL){
        free(tp);
        error();
    }
    // run manager on any thread
    for(i = 0; i < tp->numOfThreads; ++i){
        if(0 != pthread_create(&tp->threads[i], NULL, manage, (void*)tp)){
            tpDestroy(tp, 0);
            error();
        }
    }
    // return thread pool
    return tp;
}

static void* manage(void* param){

}

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks){

}

/**
 * insert function to mission queue/
 *
 * @param threadPool thread pool
 * @param computeFunc function
 * @param param parameters to function
 * @return -1 for failure, 0 for success
 */
int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param){
    // tpDestroy already called so user can't insert task
    if (threadPool->status != RUNNING) return -1;
    Task* t = (Task*)malloc(sizeof(Task));
    if (t == NULL) error();
    t->function = computeFunc;
    t->params = param;

    // insert func to queue
    pthread_mutex_lock(&(threadPool->mutex));
    osEnqueue(threadPool->queue, t);
    if (0 != pthread_cond_broadcast(&threadPool->cv)) error();
    pthread_mutex_unlock(&threadPool->mutex);
    // success
    return 0;
}
