// original
#include "server_thread.h"

#include "common.h"
#include "request.h"

// added
#include <pthread.h>

//# Self-defined Structures
struct server {
    int nr_threads;  // number of worker threads
    int max_requests;  // buffer size
    int exiting;  // server state is exiting
    int max_cache_size;  // for lab 5

    /* add any other parameters you need */
    int num_requests;  // number of requests in the buffer = in - out
    int *request_buffer;
    pthread_t **worker_threads;  // worker thread table
};

//# Global Variables
pthread_mutex_t lock;
// condition variables
pthread_cond_t full;
pthread_cond_t empty;

// in and out variable for round buffer
int in  = 0;
int out = 0;

//# static functions
static struct file_data *file_data_init(void);
static void file_data_free(struct file_data *data);
static void do_server_request(struct server *sv, int connfd);


/* initialize file data */
static struct file_data *file_data_init(void) {
    struct file_data *data;

    data            = Malloc(sizeof(struct file_data));
    data->file_name = NULL;
    data->file_buf  = NULL;
    data->file_size = 0;
    return data;
}

/* free all file data */
static void file_data_free(struct file_data *data) {
    free(data->file_name);
    free(data->file_buf);
    free(data);
}

static void do_server_request(struct server *sv, int connfd) {
    int ret;
    struct request *rq;
    struct file_data *data;

    data = file_data_init();

    /* fill data->file_name with name of the file being requested */
    rq = request_init(connfd, data);
    if (!rq) {
        file_data_free(data);
        return;
    }
    /* read file,
     * fills data->file_buf with the file contents,
     * data->file_size with file size. */
    ret = request_readfile(rq);
    if (ret == 0) { /* couldn't read file */
        goto out;
    }
    /* send file to client */
    request_sendfile(rq);
out:
    request_destroy(rq);
    file_data_free(data);
}

//# entry point functions
struct server *server_init(int nr_threads, int max_requests, int max_cache_size);
void create_worker(struct server *sv);  // helper for server_init
void server_request(struct server *sv, int connfd);
void server_exit(struct server *sv);

struct server *server_init(int nr_threads, int max_requests, int max_cache_size) {
    struct server *sv;
    sv                 = (struct server *)Malloc(sizeof(struct server));
    sv->max_requests   = max_requests;
    sv->nr_threads     = nr_threads;
    sv->exiting        = 0;
    sv->max_cache_size = max_cache_size;

    /* initialize self defined parameters num_requests, request_buffer, and worker_threads */
    if (nr_threads > 0 || max_requests > 0 || max_cache_size > 0) {
        sv->num_requests = in - out;
        pthread_mutex_init(&lock, NULL);
        pthread_cond_init(&empty, NULL);
        pthread_cond_init(&full, NULL);

        if (nr_threads <= 0)
            sv->worker_threads = NULL;
        else {
            sv->worker_threads = (pthread_t **)malloc(sizeof(pthread_t *) * nr_threads);
            for (int i = 0; i < nr_threads; i++) {
                sv->worker_threads[i] = (pthread_t *)malloc(sizeof(pthread_t));
            }
            for (int i = 0; i < nr_threads; i++) {
                pthread_create(sv->worker_threads[i], NULL, (void *)&create_worker, sv);
            }
        }

        if (max_requests <= 0)
            sv->request_buffer = NULL;
        else
            sv->request_buffer = (int *)malloc(sizeof(int) * (max_requests + 1));
    }

    /* Lab 4: create queue of max_request size when max_requests > 0 */

    /* Lab 5: init server cache and limit its size to max_cache_size */

    /* Lab 4: create worker threads when nr_threads > 0 */

    return sv;
}

void create_worker(struct server *sv) {
    while (1) {
        pthread_mutex_lock(&lock);

        while (sv->num_requests == 0 && !sv->exiting)  // no request, empty buffer, wait
            pthread_cond_wait(&empty, &lock);


        // read from buffer
        int connfd = sv->request_buffer[out];

        if (sv->num_requests == sv->max_requests)  // full buffer, wait
            pthread_cond_broadcast(&full);

        sv->num_requests -= 1;  // decrement request number
        out = (out + 1) % sv->max_requests; 

        pthread_mutex_unlock(&lock);

        if (sv->exiting) {  // exit
            pthread_mutex_unlock(&lock);
            pthread_exit(0);
        }

        do_server_request(sv, connfd);  // process request
    }
}

void server_request(struct server *sv, int connfd) {
    if (sv->nr_threads == 0) { /* no worker threads */
        do_server_request(sv, connfd);
    } else {
        /*  Save the relevant info in a buffer and have one of the
         *  worker threads do the work. */
        pthread_mutex_lock(&lock);

        while (sv->max_requests == sv->num_requests)
            pthread_cond_wait(&full, &lock);  // do not need to check exit?

        sv->request_buffer[in] = connfd;

        if (sv->num_requests == 0)
            pthread_cond_broadcast(&empty);

        sv->num_requests += 1;
        in = (in + 1) % sv->max_requests;

        pthread_mutex_unlock(&lock);
    }
}

void server_exit(struct server *sv) {
    /* when using one or more worker threads, use sv->exiting to indicate to
     * these threads that the server is exiting. make sure to call
     * pthread_join in this function so that the main server thread waits
     * for all the worker threads to exit before exiting. */
    sv->exiting = 1;

    /* make sure to free any allocated resources */
    pthread_cond_broadcast(&empty);  // no need to broadcast full

    for (int i = 0; i < sv->nr_threads; ++i) {
        pthread_join(*sv->worker_threads[i], NULL);
    }

    for (int i = 0; i < sv->nr_threads; ++i) {
        free((sv->worker_threads)[i]);
    }

    free(sv->request_buffer);
    free(sv->worker_threads);

    free(sv);
}
