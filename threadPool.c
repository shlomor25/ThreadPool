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
    // casting after passing in thread
    ThreadPool* tp = (ThreadPool*)param;
    if (tp == NULL) error();
    // while running or tpDestroy() called but queue wasn't empty
    while(tp->status == RUNNING || (!osIsQueueEmpty(tp->queue) && tp->status == FINISH)) {
        pthread_mutex_lock(&tp->mutex);
        // when queue is empty
        while (tp->status == RUNNING && osIsQueueEmpty(tp->queue)) {
            pthread_cond_wait(&tp->cv, &tp->mutex);
        }
        Task *t = osDequeue(tp->queue);
        pthread_mutex_unlock(&tp->mutex);
        // call mission
        if (t != NULL) {
            (t->function)(t->params);
            free(t);
        }
    }
    // now cpDestroy() called and queue is empty
    pthread_mutex_unlock(&tp->mutex);
    pthread_exit(NULL);
}

/**
 * Destroy thread pool.
 *
 * @param threadPool
 * @param shouldWaitForTasks
 */
void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks){
    int i;
    pthread_mutex_lock(&threadPool->mutex);
    // update status
    if (shouldWaitForTasks == 0) {
        threadPool->status = TERMINATE;
    } else {
        threadPool->status = FINISH;
    }
    pthread_cond_broadcast(&threadPool->cv);
    pthread_mutex_unlock(&threadPool->mutex);

    // wait for all threads
    for (i = 0; i < threadPool->numOfThreads; ++i){
        pthread_join(threadPool->threads[i], NULL);
    }
    // complete all task still in queue
    while (!osIsQueueEmpty(threadPool->queue)){
        Task* t = osDequeue(threadPool->queue);
        free(t);
    }
    // free all memory
    free(threadPool->threads);
    osDestroyQueue(threadPool->queue);
    pthread_cond_destroy(&threadPool->cv);
    pthread_mutex_destroy(&threadPool->mutex);
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
    pthread_mutex_lock(&(threadPool->mutex));
    osEnqueue(threadPool->queue, t);
    if (0 != pthread_cond_broadcast(&threadPool->cv)) error();
    pthread_mutex_unlock(&threadPool->mutex);
    // success
    return 0;
}
