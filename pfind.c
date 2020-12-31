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
atomic_int num_of_failed_threads = 0;

int num_of_threads = 0;
int num_of_created_threads = 0;
pthread_mutex_t num_of_created_lock;

pthread_mutex_t queue_actions_lock;
pthread_mutex_t queue_waiting_lock;


char *search_term;

pthread_t *threads;
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



//checking if the dirqueue is empty
int is_empty(){
    pthread_mutex_lock(&queue_actions_lock);
    int val = (dir_queue->head == NULL);
    pthread_mutex_unlock(&queue_actions_lock);
    return val;
}


//push a search directory to the end of queue
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
    pthread_mutex_lock(&queue_actions_lock);
    if (dir_queue->tail == NULL ){
        dir_queue->head = dir_queue->tail = temp;
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

//pop the next directory to search in from the head of the queue
char * pop (){
    num_of_running_threads--;
    pthread_mutex_lock(&queue_waiting_lock);
    while (is_empty() && num_of_running_threads>0){
        pthread_cond_wait(&empty_queue_cv, &queue_waiting_lock);
    }
    pthread_mutex_unlock(&queue_waiting_lock);
    if (num_of_running_threads == 0 && is_empty()){
        pthread_cond_signal(&empty_queue_cv);
        return NULL;
    }
    num_of_running_threads++;
    pthread_mutex_lock(&queue_actions_lock);
    queue_node *temp = dir_queue->head;
    dir_queue->head = dir_queue->head->next_node;
    if (dir_queue->head ==NULL){
        dir_queue->tail = NULL;
    }
    pthread_mutex_unlock(&queue_actions_lock);
    char *dir = temp ->dir_name;
    free(temp);
    return dir;
}




//---------the threads methods----------

//checking if a directory is readable or not
int is_readable(char *path){
    struct stat s;
    lstat(path, &s);
    return s.st_mode & S_IRUSR;
}

//return a string for the new path in a correct format
char *create_new_path (char *dir, char *entry){
    unsigned long total_length = strlen(dir) + strlen(entry) + 2;
    char *new_path = malloc(total_length*sizeof(char));
    snprintf(new_path, total_length, "%s/%s", dir, entry);
    new_path[total_length-1] = '\0';
    return new_path;
}

//handling specific entries in the directory
void handle_entry(char *dir,struct dirent *entry){
    char *cur_path;
    //ignoring the directories . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
        return;
    }
    cur_path = create_new_path(dir, entry->d_name);
    int d_type = entry->d_type;
    if (d_type ==4){
        if (is_readable(cur_path)){
            push(cur_path);
        } else {
            printf("Directory %s: Permission denied.\n", cur_path);
        }
    } else if (d_type ==8 || d_type == 10) {
        if (strstr(entry->d_name, search_term) != NULL){
            printf("%s\n", cur_path);
            matched_files_counter++;
        }
        free(cur_path);
    }
}

//iterating over all files, links and directories inside a directory using the above handle_entry method
void iterate_dir (char * dir){
    struct dirent *cur_dirent;
    DIR *iterated_dir;
    if ((iterated_dir = opendir(dir)) == NULL){
        num_of_running_threads--;
        num_of_failed_threads++;
        fprintf(stderr, "Directory %s: Permission denied.\n", dir);
        pthread_exit(NULL);
    }

    errno = 0;
    while ((cur_dirent = readdir(iterated_dir)) != NULL){
        handle_entry(dir, cur_dirent);
    }

    closedir(iterated_dir);
    if (errno !=0){
        num_of_running_threads--;
        num_of_failed_threads++;
        fprintf(stderr, "error reading from dir%s\n", dir);
        pthread_exit(NULL);
    }
}

//the main function that every new thread starts from
void *threads_main (){
    //wait until all threads created
    pthread_mutex_lock(&num_of_created_lock);
    if (num_of_threads != num_of_created_threads){
        pthread_cond_wait(&num_of_threads_cv, &num_of_created_lock);
    }
    num_of_running_threads++;
    pthread_mutex_unlock(&num_of_created_lock);
    while (1){
        char *dir = pop();
        if (dir == NULL){
            pthread_exit(NULL);
        }
        iterate_dir(dir);
        free(dir);
    }

    pthread_exit(NULL);
}


void wait_for_all_threads(pthread_t *threads) {
    for (int i = 0; i < num_of_threads ; ++i) {
        pthread_join(threads[i], NULL);
    }
}

//a function that allocates memory for an array of threads and initializing each one of them.
void create_threads (){
    threads = (pthread_t *)malloc(num_of_threads*sizeof(pthread_t ));
    if (threads == NULL){
        fprintf(stderr, "error allocating memory for threads\n");
        exit(FAILED);
    }
    for (int i=0; i<num_of_threads;i ++){
        pthread_mutex_lock(&num_of_created_lock);
        if (0 != pthread_create(&(threads[i]), NULL, threads_main, NULL)){
            fprintf(stderr, "error creating thread number %d\n", i);
            pthread_mutex_unlock(&num_of_created_lock);
            exit(FAILED);
        } else {
            num_of_created_threads ++;
            if (num_of_created_threads == num_of_threads){ //if all all threads created - signal the threads to start working
                pthread_cond_broadcast(&num_of_threads_cv);
            }
            pthread_mutex_unlock(&num_of_created_lock);
        }
    }
}

//---------general use functions----------

//initializing all the locks and the cvs
void init_locks_and_cvs(){
    int rc;

    rc = pthread_mutex_init(&num_of_created_lock, NULL);
    if (rc) {
        fprintf(stderr, "ERROR in pthread_mutex_init(): num of running threads lock\n");
        exit(FAILED);
    }

    rc = pthread_mutex_init(&queue_actions_lock, NULL);
    if (rc) {
        fprintf(stderr, "ERROR in pthread_mutex_init(): queue actions lock lock\n");
        exit(FAILED);
    }

    rc = pthread_mutex_init(&queue_waiting_lock, NULL);
    if (rc) {
        fprintf(stderr, "ERROR in pthread_mutex_init(): queue_waiting_lock lock\n");
        exit(FAILED);
    }

    rc = pthread_cond_init(&empty_queue_cv, NULL);
    if (rc) {
        fprintf(stderr, "ERROR in pthread_cond_init(): empty queue cond var lock\n");
        exit(FAILED);
    }

    rc = pthread_cond_init(&num_of_threads_cv, NULL);
    if (rc) {
        fprintf(stderr, "ERROR in pthread_cond_init(): num of threads cond var lock\n");
        exit(FAILED);
    }
}

//destroying all the locks and the cvs
void destroy_locks_and_cvs(){
    pthread_mutex_destroy(&num_of_created_lock);
    pthread_mutex_destroy(&queue_actions_lock);
    pthread_mutex_destroy(&queue_waiting_lock);
    pthread_cond_destroy(&empty_queue_cv);
    pthread_cond_destroy(&num_of_threads_cv);
}


int is_dir (char *path){
    struct stat buff;
    lstat(path, &buff);
    if (S_ISDIR(buff.st_mode)){
        return 1;
    } else {
        return 0;
    }
}

char *check_root_dir(char *arg){
    if(!is_dir(arg)){
        return NULL;
    }
    char *formatted_dir = malloc((strlen(arg)+1)*sizeof(char));
    memcpy(formatted_dir, arg, strlen(arg)+1);
    return formatted_dir;
}
//====================================================
int main(int argc, char *argv[]) {
    if (argc<3){
        fprintf(stderr, "only %d arguments submitted instead of 3\n",argc);
        exit(FAILED);
    }
    char *root_dir ;
    dir_queue = (fifo_queue *)malloc(sizeof(fifo_queue));
    if (dir_queue == NULL){
        fprintf(stderr, "FIFO queue memory allocation failed\n");
        exit(FAILED);
    }
    dir_queue->head =dir_queue->tail =  NULL;


    if ((root_dir = check_root_dir(argv[1])) == NULL){
        fprintf(stderr, "argv[1] is invalid directory\n");
        exit(FAILED);
    }
    search_term = argv[2];

    num_of_threads =(int) strtol(argv[3], NULL,10);
    if (num_of_threads == UINT_MAX && errno == ERANGE){
        fprintf(stderr, "%s is too big for an integer\n", argv[3]);
        exit(FAILED);
    }

    if ((push(root_dir)) == FAILED){
        exit(FAILED);
    }
    //initializing all the locks and the conditional variables
    init_locks_and_cvs();
    //create an array of num_of_threads threads and create them.
    create_threads();
    //wait for all the threads to finish
    wait_for_all_threads(threads);
    printf("Done searching, found %d files\n", matched_files_counter);

    //clean our working environment
    destroy_locks_and_cvs();
    free(dir_queue);
    free(threads);

    //see if any thread failed
    if (num_of_failed_threads>0){
        exit(FAILED);
    }
    exit(SUCCESS);


}
//=================== END OF FILE ====================
