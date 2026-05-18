#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>

#include "thread.h"

#define MAX_EVENTS 1024

/* Thread pool global variable  */
iThreadPool* pool = NULL;

/* Socket non-blocking setup function */
int setNonBlocking(int fd) {
    int flags = fcntl(fd ,F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void* work1(void* arg) {
    while (1) {
        pthread_mutex_lock(&(pool->lock));
        while (pool->queue_head == NULL && !pool->shutdown) {
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }
        if (pool->shutdown) {
            pthread_mutex_unlock(&(pool->lock));
            pthread_exit(NULL);
        }
        iThreadTask *task = pool->queue_head;
        if (task != NULL) {
            pool->queue_head = task->next;
        }
        pthread_mutex_unlock(&(pool->lock));

        if (task != NULL) {
            (*(task->function))(task->argument);
            free(task);
        }
    }
    return NULL;
}

void iThreadPoolInit(int num_threads) {
    pool = (iThreadPool *)malloc(sizeof(iThreadPool));
    pool->thread_count = num_threads;
    pool->shutdown = 0;
    pool->queue_head = NULL;
    pthread_mutex_init(&(pool->lock), NULL);
    pthread_cond_init(&(pool->notify), NULL);
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&(pool->threads[i]), NULL, work1, NULL);
    }
}

void iThreadPoolAdd(void (*func)(void*), void* arg) {
    iThreadTask *task = (iThreadTask *)malloc(sizeof(iThreadTask));
    task->function = func;
    task->argument = arg;
    task->next = NULL;

    pthread_mutex_lock(&(pool->lock));
    task->next = pool->queue_head;
    pool->queue_head = task;
    pthread_cond_signal(&(pool->notify));
    pthread_mutex_unlock(&(pool->lock));
}