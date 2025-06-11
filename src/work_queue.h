#pragma once
#include <pthread.h>
#include "basic_types.h"

#define NUM_THREADS 2
#define QUEUE_SIZE 10

using wq_task_func = void(void *arg);
struct wq_task
{
    void (*func)(void *arg);
    void *arg;
};

struct work_queue
{
    pthread_t workers[NUM_THREADS];
    wq_task tasks[QUEUE_SIZE];
    sizet front;
    sizet rear;
    sizet count;
    s8 stop;
    pthread_mutex_t mutex;
    pthread_cond_t task_added;
    pthread_cond_t task_removed;
};

void *worker_thread(void *arg);
void enqueue_task(work_queue *queue, wq_task task);
void init_work_queue(work_queue *queue);
void terminate_work_queue(work_queue *queue);
