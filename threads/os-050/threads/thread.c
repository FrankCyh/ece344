#include "thread.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ucontext.h>

#include "interrupt.h"


//# Self defined structure
//$ enum of state of thread t_state
typedef enum {
    READY   = 0,
    RUNNING = 1,
    EXITED  = 2,
    BLOCKED = 3
} t_state;

//$ Thread node structure
typedef struct t_node {
    Tid tid;
    struct t_node *next;
} t_node;

//$ Queue
t_node *ready_q;

// t_node *exit_q;  // not used in lab 2, use traversing to exit

//$ Thread structure
typedef struct thread {
    Tid tid;
    t_state status;
    bool killed;  // to be killed next time
    bool initialized;  // intialized before
    ucontext_t context;
    void *stackptr;
} thread;

thread *current_thread;  // one thread that is running
thread *t_array[THREAD_MAX_THREADS] = {NULL};  // table of threads

//# Directory

void insertNode(t_node *root, Tid insert_id);
void deleteNode(t_node *root, Tid delete_id);
void print_q(t_node *root);

Tid getNewTid();
Tid free_exited_thread();

void thread_stub(void (*thread_main)(void *), void *arg);
void queue_init();

// required
void thread_init(void);
Tid thread_id();
Tid thread_create(void (*fn)(void *), void *parg);
Tid thread_yield(Tid want_tid);
void thread_exit();
Tid thread_kill(Tid tid);


//# Queue manipulation
void insertNode(t_node *root, Tid insert_id) { // insert a new node at end
    if (root == NULL)
        return;

    t_node *new_thread = (t_node *)malloc(sizeof(t_node));  // new node

    // initialize new node in queue
    new_thread->tid  = insert_id;
    new_thread->next = NULL;

    while (root->next != NULL)
        root = root->next;
    root->next = new_thread;

    return;
}

void deleteNode(t_node *root, Tid delete_id) {
    if (root == NULL || root->next == NULL)
        return;  // no node to delete

    t_node *prev    = root;
    t_node *current = root->next;  // first one is dummy

    while (current != NULL) {
        if (current->tid == delete_id) {
            prev->next = current->next;
            free(current);
            return;
        }
        prev    = current;
        current = current->next;
    }
}

// //* helper function
// void print_q(t_node *root) {
//     if (root == NULL || root->next == NULL) {
//         printf("Empty queue\n");
//         return;  // no queue no times in queue
//     }

//     t_node *curr = root->next;

//     while (curr != NULL) {
//         printf("%d -> ", curr->tid);
//         curr = curr->next;
//     }
// }

//# Helper functions
// Helper for thread_main
void thread_stub(void (*thread_main)(void *), void *arg) {
    interrupts_on();
    //Tid ret;

    thread_main(arg);
    thread_exit();

    //we get here if we are on last thread
    exit(0);  //exit program
}

// Description: generate a thread id for a newly created thread
Tid getNewTid() {
    Tid i = 0;
    while (i < THREAD_MAX_THREADS && t_array[i] != NULL)
        i++;

    if (i < THREAD_MAX_THREADS)  // found available thread
        return i;

    else  // no found, i == THREAD_MAX_THREADS
        return THREAD_NOMORE;
}

// traverse the whole thread table to remove exited threads
Tid free_exited_thread() {
    for (Tid i = 0; i < THREAD_MAX_THREADS; i++) {
        while (t_array[i] == NULL)
            i++;
        if (t_array[i]->status == EXITED) {
            deleteNode(ready_q, i);
            free(t_array[i]->stackptr);  // malloced
            free(t_array[i]);  // malloced
            t_array[i] = NULL;
            return i;
        }
    }
    return THREAD_NONE;
}

//# thread_init
void queue_init() {
    ready_q      = (t_node *)malloc(sizeof(t_node));
    ready_q->tid = -1;  // dummy node

    ready_q->next      = (t_node *)malloc(sizeof(t_node));
    ready_q->next->tid = (current_thread->tid);

    // exit_q       = (t_node *)malloc(sizeof(t_node));
    // exit_q->tid  = -1;  // dummy node
    // exit_q->next = NULL;
}

void thread_init(void) {
    current_thread              = (thread *)malloc(sizeof(thread));
    current_thread->killed      = false;
    current_thread->initialized = true;
    current_thread->tid         = 0;
    current_thread->status      = RUNNING;

    t_array[0] = current_thread;

    getcontext(&(current_thread->context));  // return 0 on success

    queue_init();
}

//# thread_id
// get current thread_id
Tid thread_id() {
    int enabled = interrupts_off();
    return current_thread->tid;
    interrupts_set(enabled);
}

//# thread_create
Tid thread_create(void (*fn)(void *), void *parg) {
    int enabled = interrupts_off();

    Tid new_Tid = getNewTid();  // get available new tid

    if (new_Tid == THREAD_NOMORE)  // out of thread
        return THREAD_NOMORE;

    thread *new_thread = (thread *)malloc(sizeof(thread));
    if (new_thread == NULL) {
        return THREAD_NOMEMORY;
    }

    // new thread parameters
    new_thread->tid         = new_Tid;
    new_thread->status      = READY;
    new_thread->initialized = false;
    new_thread->killed      = false;

    // new thread stack
    void *new_stack = (void *)malloc(THREAD_MIN_STACK);
    if (new_stack == NULL) {  // malloc failed
        free(new_thread);  // free new_thread just created
        return THREAD_NOMEMORY;
    }
    new_thread->stackptr = new_stack;

    // new thread ucontext
    getcontext(&(new_thread->context));  // copy context for later activate

    new_thread->context.uc_mcontext.gregs[REG_RDI] = (unsigned long)fn;
    new_thread->context.uc_mcontext.gregs[REG_RSI] = (unsigned long)parg;
    new_thread->context.uc_mcontext.gregs[REG_RIP] = (unsigned long)thread_stub;
    new_thread->context.uc_mcontext.gregs[REG_RSP] = (unsigned long)(new_stack + THREAD_MIN_STACK - 8);

    // add new thread to ready_q
    insertNode(ready_q, new_Tid);
    t_array[new_Tid] = new_thread;

    interrupts_set(enabled);

    return new_Tid;
}


//# thread_yield
Tid thread_yield(Tid want_tid) {
    // current thread is a thread to be killed, change a new thread from ready_q as current_thread
    if (t_array[current_thread->tid]->killed) {
        deleteNode(ready_q, current_thread->tid);
        thread_exit();
    }

    Tid ready_id = want_tid;

    // THREAD_INVALID
    if (want_tid < -7 || want_tid > THREAD_MAX_THREADS)  // out of scope
        return THREAD_INVALID;

    // THREAD_SELF
    else if (want_tid == current_thread->tid || want_tid == THREAD_SELF)
        // no operation
        return current_thread->tid;

    // THREAD_ANY
    else if (want_tid == THREAD_ANY) {
        if (ready_q->next != NULL) {
            ready_id = ready_q->next->tid;  // run the thread at the head of the ready queue
            if (ready_id == current_thread->tid)  // don't understand
                return THREAD_NONE;
        } else {
            return THREAD_NONE;  // no threads in the ready_q
        }
    }

    // THREAD_INVALID
    else if (t_array[want_tid] == NULL || t_array[want_tid]->status != READY)  // Not initialized or not at a ready state
        return THREAD_INVALID;

    thread *ready_thread = t_array[ready_id];

    deleteNode(ready_q, ready_id);  // delete the running node from ready queue

    getcontext(&(t_array[current_thread->tid]->context));

    free_exited_thread();

    ready_thread->initialized = !ready_thread->initialized;

    if (!ready_thread->initialized) {
        return ready_id;

    } else {
        ready_thread->status                 = RUNNING;
        t_array[current_thread->tid]->status = READY;

        insertNode(ready_q, current_thread->tid);

        current_thread = ready_thread;

        setcontext(&(ready_thread->context));
    }

    return THREAD_NONE;
}

void thread_exit() {
    Tid ready_id = 0;

    if (ready_q == NULL || ready_q->next == NULL)
        exit(0);  // last thread exits
    else
        ready_id = ready_q->next->tid;

    free_exited_thread();

    t_array[current_thread->tid]->status = EXITED;
    t_array[ready_id]->status            = RUNNING;

    current_thread = t_array[ready_id];  // new current

    deleteNode(ready_q, ready_id);  // delete ready thread form ready queue

    setcontext(&(t_array[ready_id]->context));
}

Tid thread_kill(Tid tid) {
    if (tid < 0 || tid > THREAD_MAX_THREADS || t_array[tid] == NULL || tid == current_thread->tid)
        return THREAD_INVALID;

    t_array[tid]->killed = 1;

    return tid;
}


//# Lab 3
/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* This is the wait queue structure */
struct wait_queue {
    /* ... Fill this in Lab 3 ... */
};

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create() {
    struct wait_queue *wq;

    wq = malloc(sizeof(struct wait_queue));
    assert(wq);

    TBD();

    return wq;
}

void wait_queue_destroy(struct wait_queue *wq) {
    TBD();
    free(wq);
}

Tid thread_sleep(struct wait_queue *queue) {
    TBD();
    return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int thread_wakeup(struct wait_queue *queue, int all) {
    TBD();
    return 0;
}

/* suspend current thread until Thread tid exits */
Tid thread_wait(Tid tid) {
    TBD();
    return 0;
}

struct lock {
    /* ... Fill this in ... */
};

struct lock *
lock_create() {
    struct lock *lock;

    lock = malloc(sizeof(struct lock));
    assert(lock);

    TBD();

    return lock;
}

void lock_destroy(struct lock *lock) {
    assert(lock != NULL);

    TBD();

    free(lock);
}

void lock_acquire(struct lock *lock) {
    assert(lock != NULL);

    TBD();
}

void lock_release(struct lock *lock) {
    assert(lock != NULL);

    TBD();
}

struct cv {
    /* ... Fill this in ... */
};

struct cv *
cv_create() {
    struct cv *cv;

    cv = malloc(sizeof(struct cv));
    assert(cv);

    TBD();

    return cv;
}

void cv_destroy(struct cv *cv) {
    assert(cv != NULL);

    TBD();

    free(cv);
}

void cv_wait(struct cv *cv, struct lock *lock) {
    assert(cv != NULL);
    assert(lock != NULL);

    TBD();
}

void cv_signal(struct cv *cv, struct lock *lock) {
    assert(cv != NULL);
    assert(lock != NULL);

    TBD();
}

void cv_broadcast(struct cv *cv, struct lock *lock) {
    assert(cv != NULL);
    assert(lock != NULL);

    TBD();
}
