/**
 * Thread Pool - Stub implementation
 */

#include "log.h"
#include "memory.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

// ============================================================================
// Types
// ============================================================================

typedef void (*TaskFunc)(void* arg);

typedef struct Task {
    TaskFunc func;
    void* arg;
    struct Task* next;
} Task;

typedef struct ThreadPool {
    pthread_t* threads;
    int threadCount;
    Task* taskQueue;
    Task* taskQueueTail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool shutdown;
} ThreadPool;

// ============================================================================
// Thread Worker
// ============================================================================

static void* workerThread(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    
    while (1) {
        pthread_mutex_lock(&pool->mutex);
        
        while (!pool->taskQueue && !pool->shutdown) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }
        
        if (pool->shutdown && !pool->taskQueue) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }
        
        // Get task
        Task* task = pool->taskQueue;
        if (task) {
            pool->taskQueue = task->next;
            if (!pool->taskQueue) {
                pool->taskQueueTail = NULL;
            }
        }
        
        pthread_mutex_unlock(&pool->mutex);
        
        // Execute task
        if (task) {
            task->func(task->arg);
            velocityFree(task);
        }
    }
    
    return NULL;
}

// ============================================================================
// Public API
// ============================================================================

ThreadPool* threadPoolCreate(int numThreads) {
    if (numThreads <= 0) numThreads = 4;
    
    ThreadPool* pool = (ThreadPool*)velocityCalloc(1, sizeof(ThreadPool));
    if (!pool) return NULL;
    
    pool->threadCount = numThreads;
    pool->threads = (pthread_t*)velocityCalloc(numThreads, sizeof(pthread_t));
    
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);
    
    for (int i = 0; i < numThreads; i++) {
        pthread_create(&pool->threads[i], NULL, workerThread, pool);
    }
    
    velocityLogInfo("Thread pool created with %d threads", numThreads);
    return pool;
}

void threadPoolDestroy(ThreadPool* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
    
    for (int i = 0; i < pool->threadCount; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    // Free remaining tasks
    Task* task = pool->taskQueue;
    while (task) {
        Task* next = task->next;
        velocityFree(task);
        task = next;
    }
    
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    velocityFree(pool->threads);
    velocityFree(pool);
}

void threadPoolSubmit(ThreadPool* pool, TaskFunc func, void* arg) {
    if (!pool || !func) return;
    
    Task* task = (Task*)velocityMalloc(sizeof(Task));
    task->func = func;
    task->arg = arg;
    task->next = NULL;
    
    pthread_mutex_lock(&pool->mutex);
    
    if (pool->taskQueueTail) {
        pool->taskQueueTail->next = task;
    } else {
        pool->taskQueue = task;
    }
    pool->taskQueueTail = task;
    
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
}
