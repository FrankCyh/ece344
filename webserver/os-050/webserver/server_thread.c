// original
#include "server_thread.h"

#include "common.h"
#include "request.h"

// added
#include <pthread.h>
#include <stdbool.h>

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

struct cache_table {
    int currSize;
    struct file **hash_table;
    struct LRU *LRU;
};

struct file {  // list of file
    struct file_data *data;
    struct file *next;
};

struct LRU {  // list of files name and index, only keep the name of the file
    struct LRU *next;
    char *name;
    int size;
};

struct cache_table *cache_table;

//# Global Variables
pthread_mutex_t lock;
// condition variables
pthread_cond_t full;
pthread_cond_t empty;
pthread_mutex_t cache;

// in and out variable for round buffer
int in             = 0;
int out            = 0;
int MAX_CACHE_SIZE = 0;


//# static functions
int hashFunction(char *word);
static struct file_data *file_data_init(void);
static void file_data_free(struct file_data *data);

int hashFunction(char *word) {  //* map a file name to a index
    unsigned long hash = 5381;
    int c              = 0;

    while ((c = *word++) != '\0')
        hash = ((hash << 5) + hash) + c;

    return hash % MAX_CACHE_SIZE;
}

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

//# cache functions
void LRUAdd(struct file_data *data);
struct file *cacheLookup(char *word);


void LRUAdd(struct file_data *data) {
    if (cache_table->LRU == NULL)
        cache_table->LRU = (struct LRU *)malloc(sizeof(struct LRU));

    cache_table->LRU->size = data->file_size;
    cache_table->LRU->next = NULL;
    cache_table->LRU->name = strdup(data->file_name);
    cache_table->LRU->next = NULL;
    return;
}

struct file *cacheLookup(char *fileName) {
    int hash = hashFunction(fileName);  // turn file name into an int to find in hashtable

    if (cache_table->hash_table[hash] == NULL)
        return NULL;

    struct file *current = cache_table->hash_table[hash];

    while (current) {  //* traverse the linked list
        if (strcmp(current->data->file_name, fileName) == 0)
            return current;
        else
            current = current->next;
    }

    return NULL;
}

bool cache_evict(int fileSize) {
    int freedSize    = 0;
    int sizeOverflow = cache_table->currSize + fileSize - MAX_CACHE_SIZE;

    if (cache_table->LRU && freedSize < sizeOverflow) {
        freedSize = cache_table->LRU->size;
        cache_table->LRU = NULL;
    }

    if (freedSize >= sizeOverflow)
        return true;
    else
        return false;
}

struct file *cache_insert(const struct file_data *data) {
    if (data->file_size > MAX_CACHE_SIZE)
        return NULL;

    if (cache_table->currSize + data->file_size > MAX_CACHE_SIZE) {
        // spare space for this insert
        if (!cache_evict(data->file_size))  // if no space
            return NULL;
    }

    struct file *file_to_cache     = (struct file *)malloc(sizeof(struct file));
    file_to_cache->data            = file_data_init();
    file_to_cache->data->file_name = strdup(data->file_name);
    file_to_cache->data->file_buf  = strdup(data->file_buf);
    file_to_cache->data->file_size = data->file_size;

    file_to_cache->next = NULL;

    cache_table->currSize = cache_table->currSize + data->file_size;
    int hash              = hashFunction(data->file_name);
    LRUAdd(file_to_cache->data);

    if (cache_table->hash_table[hash] == NULL)  // null node
        cache_table->hash_table[hash] = file_to_cache;

    return file_to_cache;
}

//# entry point functions
static void do_server_request(struct server *sv, int connfd);
struct server *server_init(int nr_threads, int max_requests, int max_cache_size);
void create_worker(struct server *sv);  // helper for server_init
void server_request(struct server *sv, int connfd);
void server_exit(struct server *sv);

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

    // read file
    if (sv->max_cache_size <= 0) {
        ret = request_readfile(rq);
        if (ret == 0)
            request_destroy(rq);
        else {
            request_sendfile(rq);
            request_destroy(rq);
        }
        return;
    }

    pthread_mutex_lock(&cache);
    struct file *file_to_cache = cacheLookup(data->file_name);

    if (file_to_cache)
        request_set_data(rq, file_to_cache->data);

    if (file_to_cache == NULL) {
        pthread_mutex_unlock(&cache);
        ret = request_readfile(rq);

        if (ret == 0)  //can't read file
            goto out;

        pthread_mutex_lock(&cache);

        file_to_cache = cacheLookup(data->file_name);
        if (file_to_cache == NULL)
            file_to_cache = cache_insert(data);
        else
            request_set_data(rq, file_to_cache->data);
    }

    pthread_mutex_unlock(&cache);
    request_sendfile(rq);

out:
    request_destroy(rq);
    file_data_free(data);
}

struct server *server_init(int nr_threads, int max_requests, int max_cache_size) {
    struct server *sv;
    sv                 = (struct server *)Malloc(sizeof(struct server));
    sv->max_requests   = max_requests;
    sv->nr_threads     = nr_threads;
    sv->exiting        = 0;
    sv->max_cache_size = max_cache_size;

    MAX_CACHE_SIZE = max_cache_size;

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

        if (max_cache_size > 0) {
            cache_table           = (struct cache_table *)malloc(sizeof(struct cache_table));
            cache_table->LRU      = NULL;
            cache_table->currSize = 0;

            cache_table->hash_table = (struct file **)malloc(MAX_CACHE_SIZE * sizeof(struct file *));
            for (int i = 0; i < MAX_CACHE_SIZE; i++)
                cache_table->hash_table[i] = NULL;
        }
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

        do_server_request(sv, connfd);
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
