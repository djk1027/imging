#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>

#include "thread.h"

#define MAX_EVENTS 1024

/* Thread pool global variable */
iThreadPool* pool = NULL;

/* Socket non-blocking setup function */
int setNonBlocking(int fd) {
    int flags = fcntl(fd ,F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* Worker thread main loop function */
void* work(void* arg) {
    while (1) {
        pthread_mutex_lock(&(pool->lock));

        /* Wait for a task to arrive or a shutdown signal */
        while (pool->queue_head == NULL && !pool->shutdown) {
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }

        /* Handle pool shutdown: unlock and exit thread immediately */
        if (pool->shutdown) {
            pthread_mutex_unlock(&(pool->lock));
            pthread_exit(NULL);
        }

        /* Pop the head task from the queue */
        iThreadTask *task = pool->queue_head;
        if (task != NULL) {
            pool->queue_head = task->next;
        }

        /* Release lock before executing the task to maximize parallelism */
        pthread_mutex_unlock(&(pool->lock));

        /* Execute the captured task out of the critical section */
        if (task != NULL) {
            (*(task->function))(task->argument);
            free(task);
        }
    }
    return NULL;
}

/* Initialize the thread pool and spawn worker threads. */
void iThreadPoolInit(int num_threads) {
    /* Allocate and initialize the thread pool object */
    pool = (iThreadPool *)malloc(sizeof(iThreadPool));
    pool->thread_count = num_threads;
    pool->shutdown = 0;
    pool->queue_head = NULL;

    /* Initialize synchronization primitives and thread storage */
    pthread_mutex_init(&(pool->lock), NULL);
    pthread_cond_init(&(pool->notify), NULL);
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);

    /* Creates a working threads. */
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&(pool->threads[i]), NULL, work, NULL);
    }
}

/* Add a new task to the thread pool queue */
void iThreadPoolAdd(void (*func)(void*), void* arg) {
    /* Create and pack a new task object */
    iThreadTask *task = (iThreadTask *)malloc(sizeof(iThreadTask));
    task->function = func;
    task->argument = arg;
    task->next = NULL;

    /* Critical Section: Safely push the task to the queue head */
    pthread_mutex_lock(&(pool->lock));
    task->next = pool->queue_head;
    pool->queue_head = task;

    /* Wake up at least one sleeping worker thread */
    pthread_cond_signal(&(pool->notify));

    pthread_mutex_unlock(&(pool->lock));
}