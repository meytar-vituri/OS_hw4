#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#define FAILED 1

int matched_files_counter = 0;
pthread_mutex_t matches_lock;

int num_of_running_threads;
pthread_mutex_t num_of_running_lock;

static long TOTAL;
atomic_int counter = 1;
pthread_mutex_t lock;

//---------the queue struct and the queue methods----------
typedef struct queue_node {
    char * dir_name;
    struct queue_node *next_node;
} queue_node;

typedef struct fifo_queue {
    queue_node *head;
    queue_node *tail;
}fifo_queue;


//push a search directory into the queue
void push (fifo_queue *queue, char *dir_name ){
    if (queue == NULL){
        fprintf(stderr,"queue not initialized");
        exit(FAILED);
    }
    queue_node *temp = (queue_node *) malloc (sizeof(queue_node));
    if (temp == NULL){
        fprintf(stderr,"error allocating queue node\n");
        exit(FAILED);
    }
    temp->dir_name = dir_name;
    temp->next_node = NULL;
    if (queue->head == NULL && queue->tail == NULL){
        queue->head = queue->tail = temp;
        return;
    } else {
        queue->tail->next_node=temp;
        queue->tail = temp;
        return;
    }
}

int is_empty(fifo_queue *queue){
    return (queue->head == NULL && queue->tail == NULL);
}

//---------the threads methods----------

//a function that allocates memory for an array of threads and initializing each one of them.
void create_threads (pthread_t *threads, int num_of_threads){
    threads = (pthread_t *)malloc(num_of_threads*sizeof(pthread_t));
    if (threads == NULL){
        fprintf(stderr, "error allocating memory for threads\n");
        exit(FAILED);
    }
    for (int i=0; i<num_of_threads;i ++){
        if (0 != pthread_create(&(threads[i]), NULL, &threads_main, NULL)){
            fprintf(stderr, "error creating thread number %d\n", i);
            exit(FAILED);
        }
    }
}

void *threads_main (){
    while (){ //TODO: add 1 to the cond

    }
}


//---------general use functions----------

void init_locks(){
    int rc;
    rc = pthread_mutex_init(&matches_lock, NULL);
    if (rc) {
        printf(stderr, "ERROR in pthread_mutex_init(): matches lock\n");
        exit(FAILED);
    }

    rc = pthread_mutex_init(&num_of_running_lock, NULL);
    if (rc) {
        printf(stderr, "ERROR in pthread_mutex_init(): num of running threads lock\n");
        exit(FAILED);
    }
}
int next_counter(void) {
    // pthread_mutex_lock( &lock );
    // int temp = ++counter;
    // pthread_mutex_unlock( &lock );
    // return temp;

    return ++counter;
}

//====================================================
int is_prime(long x) {
    for (long i = 2; i < x / 2; ++i) {
        if (0 == x % i)
            return 0; // False
    }
    return 1; // True
}

//====================================================
void *prime_print(void *t) {
    long i = 0;
    long j = 0L;
    long tid = (long)t;
    double result = 0.0;

    printf("Thread %ld starting...\n", tid);

    while (j < TOTAL) {
        j = next_counter();
        ++i;
        if (is_prime(j)) {
            printf("Prime: %ld\n", j);
        }
    }

    printf("Thread %ld done.\n", tid);
    pthread_exit((void *)i);
}

//====================================================
int main(int argc, char *argv[]) {
    if (argc<3){
        fprintf(stderr, "only %d arguments submitted instead of 3\n",argc);
        exit(FAILED);
    }
    char *root_dir, *search_term;
    int num_of_threads;
    pthread_t *threads;
    fifo_queue *dir_queue = (fifo_queue *)malloc(sizeof(fifo_queue));
    if (dir_queue == NULL){
        fprintf(stderr, "FIFO queue memory allocation failed\n");
        exit(FAILED);
    }
    root_dir = argv[1];
    search_term = argv[2];
    num_of_threads = strtol(argv[3]);

    push(dir_queue, root_dir);
    //initializing all the locks
    init_locks();

    //create an array of num_of_threads threads and create them.
    create_threads(threads, num_of_threads);

    //TODO: signal the threads to start searching.


    //TODO: locks destroying function
    int rc;
    void *status;

    TOTAL = pow(10, 5);

    // --- Initialize mutex ----------------------------
    rc = pthread_mutex_init(&lock, NULL);
    if (rc) {
        printf("ERROR in pthread_mutex_init(): "
               "%s\n",
               strerror(rc));
        exit(-1);
    }

    // --- Launch threads ------------------------------
    for (long t = 0; t < NUM_THREADS; ++t) {
        printf("Main: creating thread %ld\n", t);
        rc = pthread_create(&thread[t], NULL, prime_print, (void *)t);
        if (rc) {
            printf("ERROR in pthread_create():"
                   " %s\n",
                   strerror(rc));
            exit(-1);
        }
    }

    // --- Wait for threads to finish ------------------
    for (long t = 0; t < NUM_THREADS; ++t) {
        rc = pthread_join(thread[t], &status);
        if (rc) {
            printf("ERROR in pthread_join():"
                   " %s\n",
                   strerror(rc));
            exit(-1);
        }
        printf("Main: completed join with thread %ld "
               "having a status of %ld\n",
               t, (long)status);
    }

    // --- Epilogue -------------------------------------
    printf("Main: program completed. Exiting."
           " Counter = %d\n",
           counter);

    pthread_mutex_destroy(&lock);
    pthread_exit(NULL);
}
//=================== END OF FILE ====================
