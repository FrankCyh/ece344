#include "thread.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ucontext.h>

#include "interrupt.h"

//# Self-defined structure

//$ Thread
typedef struct thread {
    Tid tid;
    ucontext_t ucontext;
    struct thread *next;
    void *stackPtr;
    bool killMe;  // thread to be killed next time
    struct wait_queue *wq;  // each thread needs its wait_q
} thread;

//$ Wait queue
typedef struct wait_queue {
    thread *wq_node;
} wait_queue;

//# Global Variables
thread *ready_queue                      = NULL;
thread *current_thread                   = NULL;
thread *thread_array[THREAD_MAX_THREADS] = {NULL};  // table of threads

//# Helper Functions for Lab2
void insertThreadToQueue(thread *queue, thread *new_thread);
thread *popFirstQueue(thread *queue);
thread *removeThreadReadyQueue(thread *queue, Tid thread_id);

void insertThreadToQueue(thread *queue, thread *new_thread) {  // used for ready_queue only
    if (new_thread == NULL)
        return;

    else if (queue->next == NULL) {
        queue->next      = new_thread;
        new_thread->next = NULL;

    } else {
        thread *tmp = queue->next;
        for (; tmp->next != NULL; tmp = tmp->next)
            ;  // get to the last node in the ready_queue

        tmp->next        = new_thread;
        new_thread->next = NULL;
    }

    return;
}

thread *popFirstQueue(thread *queue) {
    thread *tmp = queue->next;
    if (queue->next) {
        queue->next = tmp->next;
        tmp->next   = NULL;
    }
    return tmp;
}

thread *removeThreadReadyQueue(thread *queue, Tid thread_id) {
    thread *prev = NULL;
    thread *curr = queue->next;

    // traverse the linked list to find thread with tid thread_id
    while (curr && curr->tid != thread_id) {
        prev = curr;
        curr = curr->next;
    }

    if (curr->tid == thread_id) {
        if (curr == queue->next)  // head of ready_queue, need to change ready_queue
            queue->next = curr->next;
        else
            prev->next = curr->next;

        curr->next = NULL;
        return curr;
    }

    else  // curr == NULL, not found
        return NULL;
}

//# Required function
void thread_stub(void (*thread_main)(void *), void *arg) {
    interrupts_on();

    thread_main(arg);
    thread_exit();
}

void thread_init(void) {
    thread *dummy_thread   = (thread *)malloc(sizeof(thread));
    dummy_thread->tid      = 0;
    dummy_thread->stackPtr = NULL;
    dummy_thread->wq       = wait_queue_create();
    assert(!getcontext(&dummy_thread->ucontext));
    ready_queue = dummy_thread;

    current_thread           = (thread *)malloc(sizeof(thread));
    current_thread->tid      = 0;
    current_thread->stackPtr = NULL;
    current_thread->wq       = wait_queue_create();
    assert(!getcontext(&current_thread->ucontext));

    thread_array[0] = current_thread;
}

Tid thread_id() {
    return current_thread->tid;
}

Tid thread_create(void (*fn)(void *), void *parg) {
    int enabled = interrupts_set(0);

    // THREAD_NOMEMORY
    thread *new_thread = (thread *)malloc(sizeof(thread));

    if (new_thread == NULL) {
        interrupts_set(enabled);
        return THREAD_NOMEMORY;
    }

    int i = 1;
    for (; i < THREAD_MAX_THREADS; i++)
        if (thread_array[i] == NULL) {  // find a new thread_id for the new thread
            new_thread->tid = i;
            break;
        }

    // THREAD_NOMORE
    if (i == THREAD_MAX_THREADS) {
        interrupts_set(enabled);
        return THREAD_NOMORE;
    }

    assert(!getcontext(&new_thread->ucontext));

    // THREAD_NOMEMORY
    void *stack_ptr = malloc(THREAD_MIN_STACK);
    if (stack_ptr == NULL) {
        free(new_thread);
        interrupts_set(enabled);
        return THREAD_NOMEMORY;
    }
    new_thread->stackPtr = stack_ptr;

    new_thread->killMe = false;
    new_thread->next   = NULL;
    new_thread->wq     = wait_queue_create();

    // initialize registers
    new_thread->ucontext.uc_mcontext.gregs[REG_RDI] = (unsigned long)fn;
    new_thread->ucontext.uc_mcontext.gregs[REG_RIP] = (unsigned long)&thread_stub;
    new_thread->ucontext.uc_mcontext.gregs[REG_RSP] = (unsigned long)stack_ptr + THREAD_MIN_STACK - 8;
    new_thread->ucontext.uc_mcontext.gregs[REG_RSI] = (unsigned long)parg;

    // insert to ready_queue
    insertThreadToQueue(ready_queue, new_thread);
    thread_array[new_thread->tid] = new_thread;

    interrupts_set(enabled);
    return new_thread->tid;
}

Tid thread_yield(Tid want_tid) {
    int enabled = interrupts_set(0);

    // THREAD_INVALID
    if (want_tid <= -3 || want_tid >= THREAD_MAX_THREADS || (want_tid >= 0 && thread_array[want_tid] == NULL)) {
        interrupts_set(enabled);
        return THREAD_INVALID;
    }

    // THREAD_SELF
    if (want_tid == current_thread->tid)
        want_tid = THREAD_SELF;
    if (want_tid == THREAD_SELF) {
        interrupts_set(enabled);
        return current_thread->tid;
    }

    // THREAD_ANY
    if (want_tid == THREAD_ANY) {
        if (ready_queue->next) {
            volatile bool same_thread = true;
            thread *new_thread        = popFirstQueue(ready_queue);

            insertThreadToQueue(ready_queue, current_thread);
            assert(!getcontext(&current_thread->ucontext));

            if (same_thread) {
                same_thread    = !same_thread;
                current_thread = new_thread;
                if (new_thread->killMe) {
                    interrupts_set(enabled);
                    thread_exit();
                }

                setcontext(&new_thread->ucontext);
            }

            interrupts_set(enabled);
            return new_thread->tid;
        }

        // ready_q is empty, not yield
        interrupts_set(enabled);
        return THREAD_NONE;
    }

    // Normal Thread
    if (thread_array[want_tid] != NULL) {
        volatile bool same_thread = true;
        thread *new_thread        = removeThreadReadyQueue(ready_queue, want_tid);
        insertThreadToQueue(ready_queue, current_thread);
        assert(!getcontext(&current_thread->ucontext));

        if (same_thread) {
            same_thread    = !same_thread;
            current_thread = new_thread;

            // should be killed
            if (new_thread->killMe) {
                interrupts_set(enabled);
                thread_exit();  // new thread has been changed
            }

            setcontext(&new_thread->ucontext);
        }

        interrupts_set(enabled);
        return new_thread->tid;


    } else {
        // else, thread not initialized
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
}

void thread_exit() {
    int enabled = interrupts_set(0);

    // wake up the wait queue of current thread
    if (thread_array[current_thread->tid]->wq)
        thread_wakeup(thread_array[current_thread->tid]->wq, 1);

    // destroy current thread
    wait_queue_destroy(thread_array[current_thread->tid]->wq);
    thread_array[current_thread->tid]->wq = NULL;
    thread_array[current_thread->tid]     = NULL;  // current thread exited
    free(current_thread->stackPtr);
    current_thread->stackPtr = NULL;
    free(current_thread);
    current_thread = NULL;

    //kill thread and change new_thread(head of the ready_q) to current_thread
    thread *new_thread = popFirstQueue(ready_queue);
    current_thread     = new_thread;
    setcontext(&current_thread->ucontext);
    interrupts_set(enabled);
}

// Kill the thread if its in the ready_q but if shes sleeping in the waiting, we'll set her to commit the next time it runs
Tid thread_kill(Tid tid) {
    int enabled = interrupts_set(0);

    Tid tid_killed = 0;

    if (tid <= -3 || tid >= THREAD_MAX_THREADS) {
        interrupts_set(enabled);
        return THREAD_INVALID;
    } else if (tid == current_thread->tid) {
        interrupts_set(enabled);
        return THREAD_INVALID;
    } else if (thread_array[tid] == NULL) {
        interrupts_set(enabled);
        return THREAD_INVALID;


    } else {
        // find in the waiting queue
        for (int i = 0; i < THREAD_MAX_THREADS; i++) {
            thread *tmp = NULL;

            if (thread_array[i] && thread_array[i]->wq && thread_array[i]->wq->wq_node) {
                tmp = thread_array[i]->wq->wq_node;
                while (tmp != NULL && tmp->tid != tid)
                    // search for the thread to be killed
                    tmp = tmp->next;

                if (tmp->tid == tid) {  // found
                    tmp->killMe = true;  // kill next time
                    interrupts_set(enabled);
                    return tmp->tid;
                }
            }
        }

        // not found in the waiting queue, must be in the ready_q
        thread *thread_to_kill = removeThreadReadyQueue(ready_queue, tid);
        tid_killed             = thread_to_kill->tid;

        free(thread_to_kill->stackPtr);
        thread_to_kill->stackPtr = NULL;

        free(thread_to_kill);

        thread_array[tid] = NULL;
    }

    interrupts_set(enabled);
    return tid_killed;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

//# Helper function for lab 3
void insertThreadWaitQ(wait_queue *wq, thread *new_thread);
thread *clearWaitQ(wait_queue *wq);
thread *popFirstOfWaitQ(wait_queue *wq);


void insertThreadWaitQ(wait_queue *wq, thread *insert_thread) {
    if (wq == NULL)
        return;

    else if (wq->wq_node == NULL) {
        wq->wq_node         = insert_thread;
        insert_thread->next = NULL;

    } else {
        thread *tmp = wq->wq_node;
        for (; tmp->next; tmp = tmp->next)
            ;

        tmp->next           = insert_thread;
        insert_thread->next = NULL;
    }

    return;
}

thread *clearWaitQ(wait_queue *wq) {
    thread *current = wq->wq_node;

    if (current == NULL)
        return NULL;

    else {  // remove all
        wq->wq_node = NULL;
        return current;
    }
}

thread *popFirstOfWaitQ(wait_queue *wq) {
    thread *current = wq->wq_node;

    if (current == NULL)
        return NULL;

    else {  // remove the top node
        wq->wq_node   = current->next;
        current->next = NULL;
        return current;
    }
}

//# required
/* make sure to fill the wait_queue structure defined above */
wait_queue *wait_queue_create() {
    int enabled    = interrupts_set(0);
    wait_queue *wq = malloc(sizeof(wait_queue));
    assert(wq);

    wq->wq_node = NULL;

    interrupts_set(enabled);
    return wq;
}

void wait_queue_destroy(wait_queue *wq) {
    int enabled = interrupts_set(0);

    thread *curr = wq->wq_node;
    thread *next = wq->wq_node;

    while (curr) {
        next = curr->next;
        free(curr);
        curr = next;
    }

    if (wq->wq_node == NULL)
        free(wq);

    interrupts_set(enabled);
}

// put the calling thread into a wait queue
Tid thread_sleep(wait_queue *wq) {  //*
    int enabled = interrupts_set(0);

    if (wq == NULL) {
        interrupts_set(enabled);
        return THREAD_INVALID;
    }

    else if (ready_queue->next != NULL) {
        volatile bool same_thread = true;  // prevent thread change

        thread *new_thread = popFirstQueue(ready_queue);  // remove the head of ready_q

        insertThreadWaitQ(wq, current_thread);
        assert(!getcontext(&current_thread->ucontext));

        if (!same_thread) {
            interrupts_set(enabled);
            return new_thread->tid;

        } else {
            same_thread    = !same_thread;
            current_thread = new_thread;
            setcontext(&new_thread->ucontext);

            interrupts_set(enabled);
            return new_thread->tid;
        }

    } else {
        // no thread in the ready_q is ready to run, couldn't sleep, because sleeping this thread cause no other runable thread
        interrupts_set(enabled);
        return THREAD_NONE;
    }
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int thread_wakeup(wait_queue *queue, int all) {  //*
    int enabled = interrupts_set(0);
    int count   = 0;
    thread *tmp = NULL;

    if (queue == NULL) {
        // queue is invalid
        interrupts_set(enabled);
        return 0;
    }

    if (all == 1) {
        // wake up all threads in the waiting queue

        for (thread *listOfThreads = clearWaitQ(queue); listOfThreads; count++) {
            tmp           = listOfThreads;
            listOfThreads = listOfThreads->next;
            insertThreadToQueue(ready_queue, tmp);  // FIFO order
        }

        interrupts_set(enabled);
        return count;
    }

    if (all == 0) {
        // wake up the top thread in the waiting queue
        tmp = popFirstOfWaitQ(queue);
        insertThreadToQueue(ready_queue, tmp);
        count += 1;

        interrupts_set(enabled);
        return count;

    } else
        return -1;  // invalid number
}


Tid thread_wait(Tid tid) {
    int enabled = interrupts_set(0);

    bool valid_id = (tid >= 0 && tid < THREAD_MAX_THREADS) && (thread_array[tid] != NULL) && (tid != current_thread->tid);

    if (!valid_id) {
        interrupts_set(enabled);
        return THREAD_INVALID;
    }

    Tid sleep_id = thread_sleep(thread_array[tid]->wq);

    bool noMoreThread = current_thread->killMe && thread_array[current_thread->tid]->wq->wq_node == NULL && ready_queue->next == NULL;

    if (noMoreThread)
        exit(0);

    else if (current_thread->killMe)
        thread_exit();

    if (tid != current_thread->tid || thread_array[tid] == NULL) {
        interrupts_set(enabled);
        return THREAD_INVALID;
    } else {
        interrupts_set(enabled);
        return sleep_id;
    }
}

struct lock {
    bool locked;
    wait_queue *wq;
};

//# Helper for lock
bool tset(struct lock *curr_lock);


bool tset(struct lock *curr_lock) {
    bool old          = curr_lock->locked;
    curr_lock->locked = true;
    return old;
}


//# Required
struct lock *lock_create() {  //*
    int enabled = interrupts_set(0);

    struct lock *lock = (struct lock *)malloc(sizeof(struct lock));
    assert(lock);

    lock->locked = false;
    lock->wq     = wait_queue_create();

    interrupts_set(enabled);
    return lock;
}

void lock_destroy(struct lock *lock) {  //*
    int enabled = interrupts_set(0);
    assert(lock != NULL);

    if (lock->locked == false) {
        wait_queue_destroy(lock->wq);
        lock->wq = NULL;
        free(lock);
    }

    interrupts_set(enabled);
}

void lock_acquire(struct lock *lock) {  //*
    int enabled = interrupts_set(0);
    assert(lock != NULL);

    while (tset(lock))
        thread_sleep(lock->wq);

    interrupts_set(enabled);
}

void lock_release(struct lock *lock) {  //*
    int enabled = interrupts_set(0);
    assert(lock != NULL);

    if (lock->locked == true) {
        lock->locked = false;
        thread_wakeup(lock->wq, 1);
    }

    interrupts_set(enabled);
}

struct cv {
    wait_queue *wq;
};

// no need to lock cv
struct cv *
cv_create() {
    int enabled = interrupts_set(0);
    struct cv *cv;

    cv = (struct cv *)malloc(sizeof(struct cv));
    assert(cv);
    cv->wq = wait_queue_create();

    interrupts_set(enabled);
    return cv;
}

void cv_destroy(struct cv *cv) {
    assert(cv != NULL);

    if (cv->wq == NULL)
        wait_queue_destroy(cv->wq);
}

void cv_wait(struct cv *cv, struct lock *lock) {
    assert(cv != NULL);
    assert(lock != NULL);

    if (lock->locked) {
        int enabled = interrupts_set(0);
        lock_release(lock);
        thread_sleep(cv->wq);
        interrupts_set(enabled);
        lock_acquire(lock);
    }
}

void cv_signal(struct cv *cv, struct lock *lock) {
    assert(lock != NULL);
    assert(cv != NULL);

    if (lock->locked) {
        thread_wakeup(cv->wq, 0);
    }
}

void cv_broadcast(struct cv *cv, struct lock *lock) {
    assert(lock != NULL);
    assert(cv != NULL);

    if (lock->locked) {
        thread_wakeup(cv->wq, 1);
    }
}
