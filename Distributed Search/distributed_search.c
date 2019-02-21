#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>

/*
 * distributed_search summary:
 * 
 * In this module we search for files (excluding hidden files) that contains the search term in them by using multiple threads and print their destination. 
 * When finished, prints out the number of files that their name include the search term found.
 *
 * distributed_search contains struct queue  to hold the queue (linked list), inside queue there are nodes which hold a DIR and the full path to the dir 
 * 
 ***Signal handling: Upon recieving SIGINT, the program terminates and prints the number of files found untill that point
 */


typedef struct node{
    struct node* next;
    char* full_path;
}Node;

typedef struct queue{
    struct node* head; /* make a linked list of DIRs */
}Queue;



void enqueue(char *path);
void* dequeue(void* t);
void search_for_occurrence(Node* node, long tid);
void change_sigint();


static Queue que;
pthread_mutex_t lock;
pthread_mutex_t print_lock;
pthread_cond_t notEmpty;
volatile static unsigned int num_found=0;
static char* search_term, *path_to_root;
volatile static int running_threads=0;
int broadcasted=1,SIGINT_INVOKED=0;

/*
* main - Initiate mutexs, cond variables and create and join threads 
*/
int main(int argc, char** argv){
    if (argc!=4){
        fprintf(stderr,"Error: not enough arguments were given");
        return 1;
    }
    void* status=NULL; int error_counter=0; 
    que.head=NULL;
    search_term = argv[2];  path_to_root=argv[1];
    // --- Initialize mutex ----------------------------
    int ret_val = pthread_mutex_init(&lock, NULL);
    if (ret_val){
        fprintf(stderr,"%s\n", strerror(ret_val));
        return 1;
    }
    ret_val = pthread_mutex_init(&print_lock, NULL);
    if (ret_val){
        fprintf(stderr,"%s\n", strerror(ret_val));
        return 1;
    }
    // --- Initialize condition variable object ----------------------------
    ret_val = pthread_cond_init(&notEmpty, NULL);
    if (ret_val) {
        fprintf(stderr,"%s\n", strerror(ret_val));
        return 1;
    }
    int num_of_threads=atoi(argv[3]);
    if(num_of_threads<1 ) return 1;
    enqueue(argv[1]); /*enqueue search root directory */
    pthread_t* thread = (pthread_t*)malloc(sizeof(pthread_t) * num_of_threads); 
    if (thread==NULL){
        fprintf(stderr, "Allocation failure\n");
        return 1;
    }
    change_sigint();
    running_threads=num_of_threads; /*this is used to tell how many running threads currently are NOT idle */

    // --- Launch threads ------------------------------
    long i;
    for (i=0 ; i < num_of_threads ; i++){
        ret_val = pthread_create(&thread[i], NULL, dequeue, (void*)i);
        if (ret_val){
            fprintf(stderr, "%s\n", strerror(ret_val));
            return 1;
        }
    }
    
    // --- Wait for threads to finish ------------------
    for (i=0 ; i < num_of_threads ; i++){
        ret_val = pthread_join(thread[i] , &status);
        if (ret_val){
            fprintf(stderr, "%s\n", strerror(ret_val));
            return 1;
        }
        if (status!=NULL && SIGINT_INVOKED==0){
            if (strcmp(status, "1")==0) { /*means a thread exited due to an error */
                error_counter++;
            }
        }
    }
    if (error_counter==num_of_threads){ /*all threads exited due to an error */
        free(thread);
        return 1;
    }
    if (SIGINT_INVOKED){
        printf("Search stopped, found %d files\n", num_found);
    }
    else {
        printf("Done searching, found %d files\n", num_found);
    }
    ret_val = pthread_mutex_destroy(&lock);
    if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); return 1;}
    ret_val=pthread_mutex_destroy(&print_lock);  
    if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); return 1;}
    ret_val =pthread_cond_destroy(&notEmpty);
    if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); return 1;}
    free(thread);
    if (error_counter>0) return 1;
    else return 0;
}

/**
 * enqueue - adds a node atomically to the linked list
 * @param *path- the path to the DIR of the node that is pushed to the queue
 * @return void
 */
void enqueue(char* path){
    int ret_val=pthread_mutex_lock(&lock); 
    if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); __sync_fetch_and_sub(&running_threads,1); pthread_exit("1");}
    if (que.head==NULL){
        que.head=(Node*)malloc(sizeof(Node));
        if (que.head==NULL){
            fprintf(stderr, "Allocation error occured");
            pthread_exit("1");
        }
        que.head->next=NULL;
        que.head->full_path=path;
    }
    else {
        Node* temp = (Node*)malloc(sizeof(Node));
        if (temp==NULL){
            fprintf(stderr, "Allocation error occured");
            pthread_exit("1");
        }
        temp->next=que.head;
        temp->full_path=path;
        que.head=temp;
    }
    ret_val=pthread_cond_signal(&notEmpty);
    if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); __sync_fetch_and_sub(&running_threads,1); pthread_exit("1");}
    ret_val=pthread_mutex_unlock(&lock);
    if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); __sync_fetch_and_sub(&running_threads,1); pthread_exit("1");}
}

/**
 * dequeue - remove a node atomically from the linked list
 * @param *t - holds the thread id (that was created in main)
 * @post - If an error occurs, the thread exits with status "1"
 * @post - Upon recieving SIGINT, the thread terminates
 * @return void* 0
 */
void* dequeue(void* t){
    while (1){
        long tid = (long)t;
        if (SIGINT_INVOKED){
             __sync_fetch_and_sub(&running_threads,1);
            pthread_cond_broadcast(&notEmpty);
            pthread_cancel(pthread_self());
            pthread_testcancel(); /* cancelation point */
        }
        int ret_val = pthread_mutex_lock(&lock);
        if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); __sync_fetch_and_sub(&running_threads,1); pthread_exit("1");}
        __sync_fetch_and_sub(&running_threads,1);
        while (que.head==NULL){/*enter sleep */
            if (running_threads==0){ /*if entered, it means that all thread are idle (which only occurs if the search is over) */
                ret_val = pthread_mutex_unlock(&lock);
                if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); pthread_exit("1");}
                if (broadcasted){ /*only total of one broadcast is needed */
                    __sync_fetch_and_sub(&broadcasted,1);
                    ret_val=pthread_mutex_lock(&lock); if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); pthread_exit("1");}
                    ret_val = pthread_cond_broadcast(&notEmpty); /*wake up all idle threads, it is time to end */
                    if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); pthread_exit("1");}
                    ret_val = pthread_mutex_unlock(&lock);
                    if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); pthread_exit("1");}
                }
                return (void*)0;
            }
            ret_val = pthread_cond_wait(&notEmpty, &lock);
            if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); pthread_exit("1");}
            if (SIGINT_INVOKED){
                pthread_mutex_unlock(&lock);
                pthread_cond_broadcast(&notEmpty);
                pthread_cancel(pthread_self());
                pthread_testcancel(); /* cancelation point */
            }
        }
        __sync_fetch_and_add(&running_threads,1);
        /* dequeue */
        Node* removed_folder = que.head;
        if (que.head->next==NULL){ /* queue contains only a single item */
            que.head=NULL;
        }
        else {
            que.head = que.head->next;
        }
        ret_val = pthread_mutex_unlock(&lock);
        if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); __sync_fetch_and_sub(&running_threads,1); pthread_exit("1");}
        search_for_occurrence(removed_folder,tid);
    }
    return (void*)0; 
}

/**
 * search_for_occurrence - searches in the folder for a files that include in their name the search term
 * @param *node - holds the the node that is being searched
 * @param tid - holds the thread id (that was created in main)
 * @return void
 */
void search_for_occurrence(Node* node, long tid){ 
    struct stat stbuf;
    DIR* dirp = opendir(node->full_path);
    if (dirp==NULL){
        if (errno==EACCES){
            return;
        }
        else{
            fprintf(stderr, "%s\n",strerror(errno));
            __sync_fetch_and_sub(&running_threads,1);
            pthread_cond_broadcast(&notEmpty);
            free(node);
            pthread_exit("1");
        }
    }
    char *parent_path = node->full_path;
    struct dirent* dp;
    while ( (dp=readdir(dirp)) !=NULL)
    {
        if (dp->d_name[0]=='.'){ /*meaning a hidden object, "." or ".." */
            continue;
        }
        int size=strlen(parent_path) +strlen(dp->d_name);
        char *full_path;
        if (parent_path[strlen(parent_path)-1]!='/'){
            full_path = (char*)malloc(size+2);
            strcpy(full_path, parent_path);
            full_path[strlen(parent_path)] = '/';
            strcpy(full_path+strlen(parent_path)+1, dp->d_name);
        }
        else{
            full_path = (char*)malloc(size+1);
            strcpy(full_path, parent_path);
            strcpy(full_path+strlen(parent_path), dp->d_name);
        }         
        if (stat(full_path, &stbuf)==-1){
            if (errno==EACCES){
                continue;
            }
            fprintf(stderr, "%s\n",strerror(errno)); 
            __sync_fetch_and_sub(&running_threads,1);
            pthread_exit("1");
        }
        if ((stbuf.st_mode & S_IFMT) == S_IFDIR){
            enqueue(full_path); /* viewing a folder, so add to the queue*/
        }
        else{
            if (strstr(dp->d_name,search_term) !=NULL){
                int ret_val;

                
                /*print lock - valgrind with helgrind tool shouts on fprintf without a lock (although the order of printing doesn't matter to us, so this is 
                simply to avoid errors on valgrind --tool=helgrind and drd) */
                ret_val = pthread_mutex_lock(&print_lock);
                if (SIGINT_INVOKED){
                    __sync_fetch_and_sub(&running_threads,1);
                    pthread_cond_broadcast(&notEmpty);
                    pthread_mutex_unlock(&print_lock);
                    pthread_cancel(pthread_self());
                    pthread_testcancel(); /* cancelation point */
                } 
                if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); __sync_fetch_and_sub(&running_threads,1); pthread_exit("1");}
                fprintf(stdout, "%s\n",full_path); /* current object name contains the term argv[2] */
                __sync_fetch_and_add(&num_found,1);
                ret_val = pthread_mutex_unlock(&print_lock);
                if (ret_val) {fprintf(stderr, "%s\n",strerror(ret_val)); __sync_fetch_and_sub(&running_threads,1); pthread_exit("1");}
            }
            free(full_path);
        }
    }
    closedir(dirp);
    if (strcmp(path_to_root,parent_path)==0) {/* the root search directory path wasn't dynamically allocated, so no free needed */}
    else{
        free(parent_path);
    }
    free(node); 
}

/**
 * signal_handler - This function turns on SIGINT_INVOKED flag, and wakes up idle threads in order to all threads to notice and terminate as well
 * This function arguments are determinted by struct sigaction and are not used in this program
 */
void signal_handler(int signum, siginfo_t* info, void* ptr){ /* only a single thread receives the signal, so need to signal to all other threads */
    SIGINT_INVOKED=1;
    pthread_cond_broadcast(&notEmpty);
}

/**
 * change_sigint - Registers the handler for SIGINT
 * @return void
 */
void change_sigint(){
    struct sigaction sigint;
    memset(&sigint, 0 , sizeof(sigint));
    sigint.sa_sigaction = signal_handler;
    if (0!= sigaction(SIGINT, &sigint, NULL)){
        printf("%s\n",strerror(errno));
        return;
    }
}