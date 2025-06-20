#include "logging.h"
#include "work_queue.h"

void *worker_thread(void *arg)
{
    auto queue = (work_queue *)arg;
    while (1) {
        pthread_mutex_lock(&queue->mutex);

        // Work queue has been stopped!
        if (queue->stop) {
            pthread_mutex_unlock(&queue->mutex);
            break;
        }

        // Must be in a while loop because the thread can spuriously wake up, and also.. if multiple threads were
        // waiting for a task, one of the other threads could have grabbed it.. though pthread_cond_signal should really
        // only wake up one
        while (queue->count == 0 && !queue->stop) {
            pthread_cond_wait(&queue->task_added, &queue->mutex);
        }

        // Work queue has been stopped!
        if (queue->stop) {
            pthread_mutex_unlock(&queue->mutex);
            break;
        }

        dlog("Processing task at ind %u - task count: %u", queue->front, queue->count - 1);
        wq_task task = queue->tasks[queue->front];
        queue->front = (queue->front + 1) % QUEUE_SIZE;
        queue->count--;

        pthread_cond_signal(&queue->task_removed);
        pthread_mutex_unlock(&queue->mutex);
        task.func(task.arg);
    }
    return nullptr;
}

void enqueue_task(work_queue *queue, wq_task task)
{
    pthread_mutex_lock(&queue->mutex);

    // Must be in a while loop because the thread can spuriously wake up, and also.. if multiple threads were
    // waiting for a task, one of the other threads could have grabbed it.. though pthread_cond_signal should really
    // only wake up one
    while (queue->count == QUEUE_SIZE) {
        pthread_cond_wait(&queue->task_removed, &queue->mutex);
    }
    dlog("Adding task to queue at ind %u - task count: %u", queue->rear, queue->count + 1);
    queue->tasks[queue->rear] = task;
    queue->rear = (queue->rear + 1) % QUEUE_SIZE;
    queue->count++;

    pthread_cond_signal(&queue->task_added);
    pthread_mutex_unlock(&queue->mutex);
}

void init_work_queue(work_queue *queue)
{
    ilog("Initializing work queue with %d threads", NUM_THREADS);
    pthread_mutex_init(&queue->mutex, nullptr);
    pthread_cond_init(&queue->task_removed, nullptr);
    pthread_cond_init(&queue->task_added, nullptr);
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_create(&queue->workers[i], NULL, worker_thread, queue);
    }
}

void terminate_work_queue(work_queue *queue)
{
    pthread_mutex_lock(&queue->mutex);
    queue->stop = 1;
    pthread_cond_broadcast(&queue->task_added); // Wake up all threads
    pthread_mutex_unlock(&queue->mutex);
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(queue->workers[i], NULL);
    }
    pthread_cond_destroy(&queue->task_added);
    pthread_cond_destroy(&queue->task_removed);
    pthread_mutex_destroy(&queue->mutex);
}
