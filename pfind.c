#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdatomic.h>

#define FAILED 1
#define SUCCESS 0
//---------global variables and their locks----------
atomic_int matched_files_counter = 0;
atomic_int num_of_running_threads = 0;

int num_of_threads = 0;
int num_of_created_threads = 0;
pthread_mutex_t num_of_created_lock;

pthread_mutex_t queue_actions_lock;

char *search_term;

//---------conditional variables----------
pthread_cond_t empty_queue_cv;
pthread_cond_t num_of_threads_cv;


//---------the queue struct and the queue methods----------
typedef struct queue_node {
    char * dir_name;
    struct queue_node *next_node;
} queue_node;

typedef struct fifo_queue {
    queue_node *head;
    queue_node *tail;
}fifo_queue;

fifo_queue *dir_queue;




int is_empty(){
    pthread_mutex_lock(&queue_actions_lock);
    int val = (dir_queue->head == NULL && dir_queue->tail == NULL);
    pthread_mutex_lock(&queue_actions_lock);
    return val;
}


//push a search directory into the queue
int push ( char *dir_name ){
    if (dir_queue == NULL){
        fprintf(stderr,"queue not initialized");
        exit(FAILED);
    }
    queue_node *temp = (queue_node *) malloc (sizeof(queue_node));
    if (temp == NULL){
        fprintf(stderr,"error allocating queue node\n");
        return FAILED;
    }
    temp->dir_name = dir_name;
    temp->next_node = NULL;
    pthread_mutex_lock(&queue_actions_lock)
    if (dir_queue->head == NULL ){
        dir_queue->head = temp;
        dir_queue -> tail = temp ->next_node;
        pthread_cond_signal(&empty_queue_cv);
        pthread_mutex_unlock(&queue_actions_lock);
        return SUCCESS;
    } else {
        dir_queue->tail->next_node=temp;
        dir_queue->tail = temp;
        pthread_cond_signal(&empty_queue_cv);
        pthread_mutex_unlock(&queue_actions_lock);
        return SUCCESS;
    }
}

char * pop (){
    pthread_mutex_lock(&queue_actions_lock);
    num_of_running_threads--;
    if (num_of_running_threads == 0){
        //TODO: finish everything, the queue is empty and no process is running
    }
    while (is_empty()){
        pthread_cond_wait(&empty_queue_cv);
    }
    num_of_running_threads++;
    queue_node *temp = dir_queue->head;
    dir_queue->head = dir_queue->head->next_node;
    pthread_mutex_unlock(&queue_actions_lock);
    char *dir = temp ->dir_name;
    free(temp);
    return dir;
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
        pthread_mutex_lock(&num_of_created_lock);
        if (0 != pthread_create(&(threads[i]), NULL, &threads_main, NULL)){
            fprintf(stderr, "error creating thread number %d\n", i);
            pthread_mutex_unlock(&num_of_created_lock)
            exit(FAILED);
        } else {
            num_of_created_threads ++;
            num_of_running_threads++;
            if (num_of_created_threads == num_of_threads){ //if all all threads created - signal the threads to start working
                pthread_cond_broadcast(&num_of_threads_cv);
            }
            pthread_mutex_unlock(&num_of_created_lock);
        }
    }
}

void handle_dir(char *path, char *dir_name){
    //ignore the directories . and ..
    if (strcmp(dir_name, "..")== 0){
        break;
    }
    if (strcmp(dir_name, ".") == 0){
        break;
    }
    char cur_dir = malloc(PATH_MAX * sizeof(char ));
    if (!cur_dir){
        num_of_running_threads--;
        fprintf(stderr, "error in memory allocation inside a thread\n");
        pthread_exit(NULL);
    }
    strcpy(cur_dir, path);
    if ((push(cur_dir))==FAILED){
        num_of_running_threads--;
        printf(stderr, "error pushing to queue, exiting thread\n");
        pthread_exit(NULL);
    }
}

void handle_file(char *path, chat *file_name){
    //ignore the files . and ..
    if (strcmp(file_name, "..")== 0){
        break;
    }
    if (strcmp(file_name, ".") == 0){
        break;
    }
    if (strstr(file_name, search_term) != NULL){
        matched_files_counter++;
        printf("%s\n", path);
    }
}

void iterate_dir (char * dir){
    struct dirent cur_dirent;
    DIR iterated_dir;
    char path[PATH_MAX];
    if ((iterated_dir = opendir(dir)) == NULL){
        num_of_running_threads--;
        fprintf(stderr, "error opening the dir %s\n", dir);
        pthread_exit(NULL);
    }
    while ((cur_dirent = readdir(dir))!= NULL){
        struct stat st_buf;
        if (dir[strlen(dir) -1 ] != '/'){
            sprintf(path, "%s/%s", dir, cur_dirent -> d_name);
        } else {
            sprintf(path, "%s%s", dir, cur_dirent .d_name);
        }
        if (lstat(path, &st_buf) == -1 ){
            num_of_running_threads--;
            fprintf(stderr, "error in stat the file %s\n", path);
            pthread_exit(NULL);
        }
        if (S_ISDIR(stbuf.st_mode)){
            handle_dir(path, cur_dirent-> d_name)
        } else {
            handle_file (path, cur_dirent  -> d_name)
        }
    }
    closedir(iterated_dir);
}

void *threads_main (){
    //wait until all threads created
    pthread_mutex_lock(&num_of_created_lock);
    if (num_of_threads != num_of_created_threads){
        pthread_cond_wait(&num_of_threads_cv);
    }
    pthread_mutex_unlock(&num_of_created_lock);


    while (1){
        char *dir = pop(fifo_queue);
        iterate_dir(dir);
        free(dir);
    }

    pthread_exit(NULL);
}


//---------general use functions----------

void init_locks_and_cvs(){
    int rc;
    rc = pthread_mutex_init(&matches_lock, NULL);
    if (rc) {
        printf(stderr, "ERROR in pthread_mutex_init(): matches lock\n");
        exit(FAILED);
    }

    rc = pthread_mutex_init(&num_of_created_lock, NULL);
    if (rc) {
        printf(stderr, "ERROR in pthread_mutex_init(): num of running threads lock\n");
        exit(FAILED);
    }

    rc = pthread_mutex_init(&queue_actions_lock, NULL);
    if (rc) {
        printf(stderr, "ERROR in pthread_mutex_init(): queue actions lock lock\n");
        exit(FAILED);
    }

    rc = pthread_cond_init(&empty_queue_cv, NULL);
    if (rc) {
        printf(stderr, "ERROR in pthread_cond_init(): empty queue cond var lock\n");
        exit(FAILED);
    }

    rc = pthread_cond_init(&num_of_threads_cv, NULL);
    if (rc) {
        printf(stderr, "ERROR in pthread_cond_init(): num of threads cond var lock\n");
        exit(FAILED);
    }


}


//====================================================
int main(int argc, char *argv[]) {
    if (argc<3){
        fprintf(stderr, "only %d arguments submitted instead of 3\n",argc);
        exit(FAILED);
    }
    char *root_dir ;
    pthread_t *threads;
    dir_queue = (fifo_queue *)malloc(sizeof(fifo_queue));
    if (dir_queue == NULL){
        fprintf(stderr, "FIFO queue memory allocation failed\n");
        exit(FAILED);
    }
    dir_queue->head = NULL;
    dir_queue->tail = NULL;

    root_dir = argv[1];
    search_term = argv[2];
    num_of_threads = strtol(argv[3]);

    if ((push(root_dir)) == FAILED){
        exit(FAILED);
    }
    //initializing all the locks and the conditional variables
    init_locks_and_cvs();

    //create an array of num_of_threads threads and create them.
    create_threads(threads, num_of_threads);



    //TODO: locks destroying function

}
//=================== END OF FILE ====================
