#define _GNU_SOURCE


#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/queue.h>
#include <pthread.h>
#include <time.h>
#include <netinet/in.h>

#define DATA_FILE "/var/tmp/aesdsocketdata"

//A6-1 additions
//this structure contains linked list data for each thread that is split off for new connection
typedef struct {
    pthread_t threadId;
    int client_fd;
    int isComplete;
    pthread_mutex_t *pMutex;
} thread_data_t;

//typedef struct slist_data_s slist_data_t;

typedef struct node {
    thread_data_t* data;
    struct node* next;
} Node;

// Mutex to protect the linked list
pthread_mutex_t listMutex = PTHREAD_MUTEX_INITIALIZER;
// Mutex to protect file access
pthread_mutex_t fileMutex = PTHREAD_MUTEX_INITIALIZER;

//thread functions
void* client_handler(void* arg);
void* timestamp(void *thread_param);
void insertNode(thread_data_t* data);
void removeCompletedThreads();
int make_Daemon(void);
static void signal_handler (int signal_number);
//void do_shutdown(void);