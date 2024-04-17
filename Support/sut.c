#define _XOPEN_SOURCE 600
#include <stdbool.h>
#include <unistd.h>
#include<ucontext.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h>

#include "sut.h"
#include "queue/queue.h"

#define STACK_SIZE 2048*2048

// TCP: Thread Control Block
typedef struct TCP {
    enum { OPEN, READ, WRITE, CLOSE } type;
    char* path; // For open
    int fd;     // For read, write, close
    char* buf;  // For read, write
    int size;   // For read, write
    ucontext_t* context; // User-level thread context
    bool done;  // For read, write
} TCP;


pthread_t *cexec, *iexec;
int *arr;

// ready queue and wait queue
struct queue rq, wq;

// ready queue lock and wait queue lock, and teardown
pthread_mutex_t *mutex_wq, *mutex_rq, *mutex_td;

// flag (only used once) to terminate program
bool teardown = false;

// ucp for cexec and iexec
ucontext_t *ucpe, *ucpi;

// pointer to a void function that takes int as arg
typedef void (*sut_task_f)();


bool get_teardown_flag() {
    bool flag_value;
    pthread_mutex_lock(mutex_td);
    flag_value = teardown;
    pthread_mutex_unlock(mutex_td);
    return flag_value;
}

/**
 * @brief Creates an additional iexec executor (pthread). The executor continuously
 * checks if there's a I/O thread to run: if there's one, it runs it; otherwise, pauses
 * for a short time.
 * @return void* 
 */
void *iexec_thread()
{
    while (!get_teardown_flag())
    {
        // first checks if the wait q is empty or not
        pthread_mutex_lock(mutex_wq);
        struct queue_entry *top = queue_peek_front(&wq);
        pthread_mutex_unlock(mutex_wq);
        if (top != NULL) {
            // get the ucontext_t from the entry
            ucontext_t *ucp = (ucontext_t *)top->data;
            // swap the context
            if (swapcontext(ucpi, ucp) == -1)
            {
                perror("swapcontext");
                exit(EXIT_FAILURE);
            };
            // thread no longer running: omit it
            pthread_mutex_lock(mutex_wq);
            top = queue_pop_head(&wq);
            pthread_mutex_unlock(mutex_wq);
            free(top->data);
            free(top);            
        } else {
            // sleep 100ms if there's no tasks currently
            usleep(100000);
        }
    }
}

/**
 * @brief Similar to cexec_thread(): continuously checks if there's a task in ready queue to run
 * 
 */
void cexec_thread()
{
    while (!get_teardown_flag())
    {
        // first checks if the wait q is empty or not
        pthread_mutex_lock(mutex_rq);
        struct queue_entry *top = queue_peek_front(&rq);
        pthread_mutex_unlock(mutex_rq);
        if (top != NULL) {
            // get the ucontext_t from the entry
            ucontext_t *ucp = (ucontext_t *)top->data;
            // swap the context
            if (swapcontext(ucpe, ucp) == -1)
            {
                perror("swapcontext");
                exit(EXIT_FAILURE);
            };
            // thread no longer running: omit it
            pthread_mutex_lock(mutex_rq);
            top = queue_pop_head(&rq);
            pthread_mutex_unlock(mutex_rq);
            ucontext_t *curr = (ucontext_t *)top->data;
            free(curr->uc_stack.ss_sp);
            free(curr);
            free(top);
        } else {
            // sleep 100ms if there's no tasks currently
            usleep(1000);
        }
    }
}

void sut_init()
{
    // create the ready queue
    rq = queue_create();
    queue_init(&rq);
    // create the wait queue
    wq = queue_create();
    queue_init(&wq);
    // initialize locks for queues
    mutex_rq = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    mutex_wq = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    mutex_td = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(mutex_rq, NULL);
    pthread_mutex_init(mutex_wq, NULL);
    pthread_mutex_init(mutex_td, NULL);
    ucpe = (ucontext_t *)malloc(sizeof(ucontext_t));
    ucpe->uc_stack.ss_sp = (char *)malloc(STACK_SIZE);
    ucpe->uc_stack.ss_size = STACK_SIZE;
    ucpe->uc_stack.ss_flags = 0;
    ucpe->uc_link = NULL;
    if (getcontext(ucpe) == -1)
    {
        perror("getcontext");
        exit(EXIT_FAILURE);
    };
    ucpi = (ucontext_t *)malloc(sizeof(ucontext_t));
    ucpi->uc_stack.ss_sp = (char *)malloc(STACK_SIZE);
    ucpi->uc_stack.ss_size = STACK_SIZE;
    ucpi->uc_stack.ss_flags = 0;
    ucpi->uc_link = NULL;
    // get current context for method
    if (getcontext(ucpi) == -1)
    {
        perror("getcontext");
        exit(EXIT_FAILURE);
    };
    // create the iexec thread and let it run
    iexec = (pthread_t *)malloc(sizeof(pthread_t));
    pthread_create(iexec, NULL, iexec_thread, NULL);
    // current thread is set to cexec: let it run as it should
    cexec = (pthread_t *)malloc(sizeof(pthread_t));
    pthread_create(cexec, NULL, (void *)&cexec_thread, NULL);
}


bool sut_create(sut_task_f fn) {    
    // create + allocate memory for thread
    ucontext_t *ucp = (ucontext_t *)malloc(sizeof(ucontext_t));
    // get current context for method
    if (getcontext(ucp) == 0) {
        // initialize default config for thread
        ucp->uc_stack.ss_sp = (char *)malloc(STACK_SIZE); 
        ucp->uc_stack.ss_size = STACK_SIZE;
        ucp->uc_stack.ss_flags = 0;
        // always come back to exec once the thread is done
        ucp->uc_link = ucpe;
        makecontext(ucp, fn, 0);
        // create node for u_context and add it to the ready queue (requires lock/unlock)
        struct queue_entry *entry = queue_new_node(ucp);
        pthread_mutex_lock(mutex_rq);
        queue_insert_tail(&rq, entry);
        pthread_mutex_unlock(mutex_rq);
        return true;
    } else {
        perror("getcontext");
        return false;
    }
}


void sut_yield() {
    // lock the ready queue
    pthread_mutex_lock(mutex_rq);
    // get current operation : top of ready queue
    struct queue_entry *top = queue_pop_head(&rq);
    // Save the current context and make a new node out of it
    ucontext_t *current_context = (ucontext_t *) malloc(sizeof(ucontext_t));
    getcontext(current_context);
    struct queue_entry *entry = queue_new_node(current_context);
    // add the current context to the back of ready queue
    queue_insert_tail(&rq, entry);
    // unlock the ready queue
    pthread_mutex_unlock(mutex_rq);
    // swap the context to main thread but with cexec_thread as the function
    makecontext(ucpe, cexec_thread, 0);
    swapcontext(current_context, ucpe);
}

void sut_exit() {
    // lock the ready queue
    pthread_mutex_lock(mutex_rq);
    // get current operation : top of ready queue
    struct queue_entry *top = queue_pop_head(&rq);
    // unlock the ready queue
    pthread_mutex_unlock(mutex_rq);
    getcontext(ucpe);
    // swap the context to main thread but with cexec_thread as the function
    makecontext(ucpe, (sut_task_f)cexec_thread, 0);
    swapcontext(top->data, ucpe);
};

int sut_open(char *file_name) {
    // Step 1: yield execution to C-Exec
    // lock the ready queue
    pthread_mutex_lock(mutex_rq);
    // get current operation : top of ready queue
    struct queue_entry *top = queue_pop_head(&rq);
    // Save the current context and make a new node out of it
    ucontext_t *current_context = (ucontext_t *) malloc(sizeof(ucontext_t));
    getcontext(current_context);
    struct queue_entry *entry = queue_new_node(current_context);
    // add the current context to the back of ready queue
    queue_insert_tail(&rq, entry);
    // unlock the ready queue
    pthread_mutex_unlock(mutex_rq);
    // swap the context to main thread but with cexec_thread as the function

    // Step 2: create a new context for the I/O thread
    ucontext_t *ucp = (ucontext_t *)malloc(sizeof(ucontext_t));
    // get current context for method
    if (getcontext(ucp) == 0) {
        // initialize default config for thread
        ucp->uc_stack.ss_sp = (char *)malloc(STACK_SIZE); 
        ucp->uc_stack.ss_size = STACK_SIZE;
        ucp->uc_stack.ss_flags = 0;
        // always come back to exec once the thread is done
        ucp->uc_link = ucpi;
        // make the context
        makecontext(ucp, (sut_task_f)cexec_thread, 0);
        // create node for u_context and add it to the ready queue (requires lock/unlock)
        struct queue_entry *entry = queue_new_node(ucp);
        pthread_mutex_lock(mutex_wq);
        queue_insert_tail(&wq, entry);
        pthread_mutex_unlock(mutex_wq);
    } else {
        perror("getcontext");
        return -1;
    }

    makecontext(ucpe, cexec_thread, 0);
    swapcontext(current_context, ucpe);
}

void sut_close(int fd) {}

void sut_write(int fd, char *buf, int size) {

}

char *sut_read(int fd, char *buf, int size);


void sut_shutdown() {
    // Cleanup the ready queue
    while (queue_peek_front(&rq) != NULL) {
        usleep(1000);
    }
    // Cleanup the wait queue
    while (queue_peek_front(&wq) != NULL) {
        usleep(1000);
    }
    // Signal to the threads that it's time to shut down
    pthread_mutex_lock(mutex_td);
    teardown = true;
    pthread_mutex_unlock(mutex_td);
    // Join the cexec and iexec threads to ensure they've completed
    pthread_join(*cexec, NULL);
    pthread_join(*iexec, NULL);
    // Destroy the mutexes
    pthread_mutex_destroy(mutex_rq);
    pthread_mutex_destroy(mutex_wq);
    pthread_mutex_destroy(mutex_td);
    // Free mutex memory
    free(mutex_rq);
    free(mutex_wq);
    free(mutex_td);
    // Free thread pointers
    free(cexec);
    free(iexec);
    // Free ucontext pointers
    free(ucpe->uc_stack.ss_sp);
    free(ucpe);
    free(ucpi->uc_stack.ss_sp);
    free(ucpi);
}