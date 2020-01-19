#include "queue.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0)


/*
 * Queue - the abstract type of a concurrent queue.
 * You must provide an implementation of this type 
 * but it is hidden from the outside.
 */
typedef struct QueueStruct {
    int size;        // max number of elements in the queue
    void** elements; // circular buffer of void*, each representing an element in the queue
    int read_index;  // index of the next element to be read
    int write_index; // index where the next element should be read from
    sem_t mutex;     // for mutual exclusion of accessing queue
    sem_t sem_read;  // number of elements available for reading
    sem_t sem_write; // number of elements for which there is space in the queue
} Queue;


/**
 * Allocate a concurrent queue of a specific size
 * @param size - The size of memory to allocate to the queue
 *               (number of elements the queue can contain)
 * @return queue - Pointer to the allocated queue
 */
Queue *queue_alloc(int size) {
    Queue* queue = malloc(sizeof(Queue));

    queue->size = size;

    queue->elements = (void**)malloc(size * sizeof(void*));
    if (!queue->elements) {
        handle_error("malloc");
    }

    queue->read_index = 0;
    queue->write_index = 0;

    if (sem_init(&queue->mutex, 0, 1) != 0) {
        handle_error("sem_init mutex");
    }

    if (sem_init(&queue->sem_read, 0, 0) != 0) {
        handle_error("sem_init sem_read");
    }

    if (sem_init(&queue->sem_write, 0, size) != 0) {
        handle_error("sem_init sem_write");
    }

    return queue;
}


/**
 * Free a concurrent queue and associated memory 
 *
 * Don't call this function while the queue is still in use.
 * (Note, this is a pre-condition to the function and does not need
 * to be checked)
 * 
 * @param queue - Pointer to the queue to free
 */
void queue_free(Queue *queue) {
    free(queue->elements);
    free(queue);
}


/**
 * Place an item into the concurrent queue.
 * If no space available then queue will block
 * until a space is available when it will
 * put the item into the queue and immediately return
 *  
 * @param queue - Pointer to the queue to add an item to
 * @param item - An item to add to queue. Uses void* to hold an arbitrary
 *               type. User's responsibility to manage memory and ensure
 *               it is correctly typed.
 */
void queue_put(Queue *queue, void *item) {
    sem_wait(&queue->sem_write);
    sem_wait(&queue->mutex);

    queue->elements[queue->write_index] = item;
    queue->write_index = (queue->write_index + 1) % queue->size;

    sem_post(&queue->mutex);
    sem_post(&queue->sem_read);
}


/**
 * Get an item from the concurrent queue
 * 
 * If there is no item available then queue_get
 * will block until an item becomes available when
 * it will immediately return that item.
 * 
 * @param queue - Pointer to queue to get item from
 * @return item - item retrieved from queue. void* type since it can be 
 *                arbitrary 
 */
void *queue_get(Queue *queue) {
    sem_wait(&queue->sem_read);
    sem_wait(&queue->mutex);

    void* item = queue->elements[queue->read_index];
    queue->read_index = (queue->read_index + 1) % queue->size;

    sem_post(&queue->mutex);
    sem_post(&queue->sem_write);

    return item;
}

