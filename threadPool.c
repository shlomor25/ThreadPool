/*
 *  Name: Shlomo Rabinovich
 *  ID:   308432517
 */

#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <pthread.h>
#include "threadPool.h"
#include "osqueue.h"

#define STDERR 2
#define EXIT (-1)
#define ERRMSG "Error in system call\n"

static void* manage(void* param);

/**
 * error in system call.
 *
 * write message to stderr
 * then, exit
 */
void error(){
    write(STDERR, ERRMSG, strlen(ERRMSG));
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
    if (tp == NULL) {
        free(tp);
        error();
    }
    tp->queue = osCreateQueue();
    tp->numOfThreads = numOfThreads;
    tp->status = RUNNING;
    if (0 != pthread_mutex_init(&tp->mutex, NULL)) error();
    if (0 != pthread_cond_init(&tp->cv, NULL)) error();
    tp->threads = (pthread_t*)malloc(sizeof(pthread_t)*numOfThreads);
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
    // casting after passing in thread
    ThreadPool* tp = (ThreadPool*)param;
    if (tp == NULL) error();
    // while running or tpDestroy() called but queue wasn't empty
    while(tp->status == RUNNING || (!osIsQueueEmpty(tp->queue) && tp->status == FINISH)) {

        if (0 != pthread_mutex_lock(&tp->mutex)) error();
        // when queue is empty
        while (tp->status == RUNNING && osIsQueueEmpty(tp->queue)) {
            if (0 != pthread_cond_wait(&tp->cv, &tp->mutex)) error();
        }
        Task *t = osDequeue(tp->queue);
        if (0!= pthread_mutex_unlock(&tp->mutex)) error();
        // call mission
        if (t != NULL) {
            (t->function)(t->params);
            free(t);
        }
    }
    pthread_exit(NULL);
}

/**
 * Destroy thread pool.
 *
 * @param threadPool
 * @param shouldWaitForTasks
 */
void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks){
    // if tpDestroy() already called from other thread
    if (threadPool->status == FINISH) return;
    int i;
    if (0!= pthread_mutex_lock(&threadPool->mutex)) error();
    // update status
    if (shouldWaitForTasks == 0) {
        while (!osIsQueueEmpty(threadPool->queue)){
            Task* t = osDequeue(threadPool->queue);
            free(t);
        }
    }
    threadPool->status = FINISH;
    if (0!= pthread_mutex_unlock(&threadPool->mutex)) error();
    if (0!= pthread_cond_broadcast(&threadPool->cv)) error();

    // wait for all threads
    for (i = 0; i < threadPool->numOfThreads; ++i){
        if (0 != pthread_join(threadPool->threads[i], NULL)) error();
    }
    // free all memory
    free(threadPool->threads);
    osDestroyQueue(threadPool->queue);
    if (0 != pthread_cond_destroy(&threadPool->cv)) error();
    if (0 != pthread_mutex_destroy(&threadPool->mutex)) error();
    free(threadPool);
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

    // insert function to queue
    if (0 != pthread_mutex_lock(&(threadPool->mutex))) error();
    osEnqueue(threadPool->queue, t);

    if (0 != pthread_cond_broadcast(&threadPool->cv)) error();
    if (0 != pthread_mutex_unlock(&threadPool->mutex)) error();
    // success
    return 0;
}
