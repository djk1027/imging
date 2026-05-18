#include <pthread.h>

typedef struct iThreadTask {
    void (*function)(void *arg);
    void *argument;
    struct iThreadTask *next;
} iThreadTask;

typedef struct iThreadPool {
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t *threads;
    iThreadTask *queue_head;
    int thread_count;
    int shutdown;
} iThreadPool;

int setNonBlocking(int fd);

/* Thread Pool function */
void iThreadPoolInit(int num_threads);
void iThreadPoolAdd(void (*func)(void*), void* arg);

/* Client handler */
void handleClientIo(void *arg);

/* The Task to be performed by the worker thread */
typedef struct {
    int client_fd;
} ClientContext;
