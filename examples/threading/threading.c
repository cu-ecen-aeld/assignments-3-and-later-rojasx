#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

typedef struct thread_data thread_data_t;

void* threadWait(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    thread_data_t* thread_func_args = (thread_data_t *) thread_param;

    // Wait to obtain mutex
    usleep(thread_func_args->wait_to_obtain_ms);
    
    // Obtain mutex
    if (pthread_mutex_lock(thread_func_args->mutex))
    {
        return thread_param;
    }
    
    // Wait to release
    usleep(thread_func_args->wait_to_release_ms);
    
    // Release
    if (pthread_mutex_unlock(thread_func_args->mutex))
    {
        return thread_param;
    }

    // if completed successfully:
    thread_func_args->thread_complete_success = true;
    return thread_param;
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    
    // Allocate space for the thread
    thread_data_t *thread_struct = (thread_data_t *) malloc(sizeof(thread_data_t));
    thread_struct->mutex = mutex;
    thread_struct->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_struct->wait_to_release_ms = wait_to_release_ms;
    thread_struct->thread_complete_success = false;

    // Start threadfunc (nonblocking)
    int tid = pthread_create(thread, NULL, threadWait, (void *)thread_struct);
    if (tid != 0)
    {
        return false;
    }

    // // non-blocking
    // if (pthread_detach(*thread) != 0)
    // {
    //     return false;
    // }

    return true;
}

