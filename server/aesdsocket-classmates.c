#include "aesdsocket-classmates.h"

//Global variables
bool isDaemon;
bool isError=false;

//Socket variables
//int sock_fd, client_fd;

//Thread variables
//pthread_mutex_t lock;
//thread_data_t time_thread_data;
//SLIST_HEAD(slisthead, slist_data_s) head;
Node* head =NULL;

//File variables
FILE *file;

int main(int argc, char *argv[]) {
  isDaemon=false;
  struct sigaction new_action;
  

  //Catch daemon flag(s)
  if (argc > 1 && strcmp(argv[1], "-d") == 0)
  {
    printf("it's a Daemon\n");
    isDaemon = true;
  }
  
  //Set up the signal handling
  memset(&new_action, 0, sizeof(struct sigaction));
  new_action.sa_handler=signal_handler;
  if (sigaction(SIGTERM, &new_action, NULL) != 0) {
    fprintf(stderr, "Error %d registering for SIGTERM", errno);
  } 
  if (sigaction(SIGINT, &new_action, NULL) != 0) {
    fprintf(stderr, "Error %d registering for SIGINT", errno);
  }

int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Create the server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return 1;
    }

    // Set options for reuseable address
    int opt = 1;
    if ((setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int))) == -1) {
        perror("socket options");
        return 1;
    }


    // Bind the server socket to a port
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server is listening on port 9000\n");

    // Create a thread for timestamping
    pthread_t timestamp_thread;
    if (pthread_create(&timestamp_thread, NULL, timestamp, NULL) != 0) {
        perror("pthread_create for timestamp");
        return 1;
    }

    while (!isError) {
        // Accept a new client connection
        if ((client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len)) == -1) {
            perror("accept");
            continue;
        }

        // Create a thread to handle the client
        thread_data_t* data = (thread_data_t*)malloc(sizeof(thread_data_t));
        data->client_fd = client_fd;
        data->isComplete = 0;

        if (pthread_create(&(data->threadId), NULL, client_handler, data) == 0) {
            insertNode(data);
        } else {
            perror("pthread_create");
            free(data);
        }

        // Check for and remove completed threads
        removeCompletedThreads();
    }

    // Close the server socket (not reached in this example)
    remove("/var/tmp/aesdsocketdata)");
    close(server_fd);

    return 0;
}
    
    
void* client_handler(void *arg)
{
   thread_data_t* thread_data = (thread_data_t*)arg;
    
    // Handle the client connection, e.g., read and write data
    char buffer[1024];
    ssize_t bytes_received;

    pthread_mutex_lock(&fileMutex); // Lock for file access
    file = fopen(DATA_FILE, "a+");
    while ((bytes_received = recv(thread_data->client_fd, buffer, sizeof(buffer), 0)) > 0) {
            fwrite(buffer, 1, bytes_received, file);

        if (buffer[bytes_received-1]=='\n') {
            break;
        }
    }
    fclose(file);
    pthread_mutex_unlock(&fileMutex); // Unlock file access

    char* line = NULL;
    size_t len = 0;
    ssize_t read_size = 0;

    pthread_mutex_lock(&fileMutex); // Lock for file access
    file = fopen(DATA_FILE, "a+");
    while ((read_size = getline(&line, &len, file))  !=-1) {
            send(thread_data->client_fd, line, read_size, 0);
    }
    fclose(file);
    pthread_mutex_unlock(&fileMutex); // Unlock file access

    // Mark the thread as complete
    thread_data->isComplete = 1;

    close(thread_data->client_fd);
    //pthread_exit(NULL);
    return NULL;
}

static void signal_handler (int signal_number) {
  syslog(LOG_INFO, "Caught signal, exiting");
  isError=true;
  removeCompletedThreads();
      remove("/var/tmp/aesdsocketdata)");
  //do_shutdown();
}

int make_Daemon(void) {
    pid_t pid = fork();
    if (pid < 0) {
      fprintf(stderr, "couldn't fork");
      return -1;
    }
    if (pid > 0) 
    { //we're in the parent so exit
      exit(0);
    }
    // if in child, create new SID
    setsid();
    chdir("/");
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    return 0;
}


void* timestamp(void * arg){
    while (1) {
        sleep(10); // Wait for 10 seconds

        pthread_mutex_lock(&fileMutex); // Lock for file access
        FILE* file = fopen(DATA_FILE, "a+");
        if (file != NULL) {
            time_t rawtime;
            struct tm* timeinfo;

            time(&rawtime);
            timeinfo = localtime(&rawtime);

            char timestamp[30];
            strftime(timestamp, sizeof(timestamp),"%a, %d %b %Y %H:%M:%S %z", timeinfo);

            fprintf(file, "timestamp: %s\n", timestamp);
            fclose(file);
        }
        pthread_mutex_unlock(&fileMutex); // Unlock file access
    }

}

/*
void do_shutdown(void) {
  //join threads
  slist_data_t *datap;
  void * thread_return = NULL;
  if (pthread_join(time_thread_data.threadId, &thread_return)==-1)
  {
    fprintf(stderr, "error joining timestamp thread\n");
  }
  //free(time_thread_data);
  SLIST_FOREACH(datap, &head, entries)
  {
    close(datap->client_fd);
    if (pthread_join(datap->threadId, &thread_return)==-1)
    {
      fprintf(stderr, "error joining thread\n");
    }
    free(datap);
  }
  //cleanup sockets
  close(sock_fd);
  close(client_fd);

  //cleanup files
  fclose(fptr); 
  remove("/var/tmp/aesdsocketdata");
}
*/

void insertNode(thread_data_t* data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->data = data;
    newNode->next = NULL;

    pthread_mutex_lock(&listMutex);

    if (head == NULL) {
        head = newNode;
    } else {
        Node* current = head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newNode;
    }

    pthread_mutex_unlock(&listMutex);
}

void removeCompletedThreads() {
    pthread_mutex_lock(&listMutex);

    Node* current = head;
    Node* prev = NULL;

    while (current != NULL) {
        if (current->data->isComplete) {
            // Join and cleanup the thread
            pthread_join(current->data->threadId, NULL);

            // Remove the node from the list
            if (prev == NULL) {
                head = current->next;
                free(current->data);
                free(current);
                current = head;
            } else {
                prev->next = current->next;
                free(current->data);
                free(current);
                current = prev->next;
            }
        } else {
            prev = current;
            current = current->next;
        }
    }

    pthread_mutex_unlock(&listMutex);
}