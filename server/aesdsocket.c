#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/queue.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/time.h>


#define LOG_FILE "/var/tmp/aesdsocketdata"
#define PORT 9000
#define BUFFERSIZE 20480 

struct sockaddr_in server_sockaddr, client_sockaddr;
int serv_sock;
char replyMessage;

atomic_int is_canceled = 0;
FILE *serverfile = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct _thread_args
{
    pthread_mutex_t * mutex;
    atomic_int * canceled;
    atomic_int finished;
    int sock_fd;
} thread_args;

typedef struct node
{   
    TAILQ_ENTRY(node) nodes;
    pthread_t thread;

    thread_args args;
} list_node;

void sigint_handler(int signum) {
    syslog(LOG_INFO, "Caught signal, exiting");
    //fclose(fp);
    close(serv_sock);
    //close(client_sock);
    remove(LOG_FILE); 
    closelog();
    is_canceled = 1;
}

void * worker(void * threadparam)
{
     char buffer_recv[BUFFERSIZE];
    char buffer_send[BUFFERSIZE];
    char buffer_packet[BUFFERSIZE];
    ssize_t recvfd = 0;
    int filesize = 0;
    ssize_t packetsize = 0;
    int sendret=0;

    buffer_recv[0]='\0';
    buffer_send[0]='\0';
    buffer_packet[0]='\0';
    thread_args *e = (thread_args *)threadparam;

    //while loop to receive data from client, write to txt file, and then send back
    while ((recvfd = recv(e->sock_fd, buffer_recv, BUFFERSIZE - 1, 0)) > 0)
    {
        //check if full packet was received until you find a newline character
        if(buffer_recv[recvfd-1]!='\n')
        {
            //printf("Received packet of size %zu \n", recvfd);
            //printf("%s",buffer_recv);
            printf("Did not receive full packet.\n");
            for(ssize_t i=0; i < (recvfd); i++)
            {
            buffer_packet[i+packetsize] = buffer_recv[i];
            }
                
            packetsize += recvfd;
                
            //buffer_recv[0]='\0';  
        }
        else //end of packet found, go into this part
        {
            //printf("Received packet of size %zu \n", recvfd);
            //printf("Received full packet of size %zu \n", (recvfd + packetsize));
            printf("Received full packet \n");
                
            for(ssize_t i=0; i < (recvfd); i++)
            {
                buffer_packet[i+packetsize] = buffer_recv[i];
            }

            packetsize += recvfd;

            buffer_packet[packetsize] = '\0';

            //open file to only append
            pthread_mutex_lock(e->mutex);

            printf("Opening file \n");
            serverfile = fopen(LOG_FILE, "a");
            if (serverfile < 0)
            {  
                syslog(LOG_ERR, "Failed to open file to write: %s", strerror(errno));
                printf("Failed to open file \n");
                goto conn_fail;
            }

            //write buffer value to serverfile before resetting buffer and closing file
            fprintf(serverfile,"%s", buffer_packet);
            fclose(serverfile);

            //open file to read only
            serverfile = fopen(LOG_FILE, "r");
            filesize = fread(buffer_send, 1, BUFFERSIZE, serverfile);
            syslog(LOG_INFO, "Success: Read file");
            printf("Success, read file.\n");
            fclose(serverfile);

            pthread_mutex_unlock(e->mutex);


            //Sending file to client
            syslog(LOG_INFO, "Sending file to client");
            printf("Sending file to client \n");
            //printf("Sending: %s", buffer_send);
            sendret = send(e->sock_fd,buffer_send,filesize,0);
            if(sendret == -1)
            {
                syslog(LOG_ERR, "Failed to send file to client: %s", strerror(errno));
                printf("Failed to send file to client \n");
            }
                
            packetsize = 0;
            
        }             

    }

    e->finished = 1;

    return threadparam;

    conn_fail:
        pthread_mutex_unlock (e->mutex);
        e->finished = 1;

        return threadparam;
}

void * timer(void * arg)
{
    char output[200];
    int ret=0;
    time_t t;
    struct tm *buff;


    thread_args * e = (thread_args *)arg;
    

    while(!is_canceled)
    {
        //sleep for the first 10 seconds to avoid tripping up the autotest
        sleep(10);

        t=time(NULL);
        buff = localtime(&t);
        if(buff == NULL){
            syslog(LOG_ERR, "Failure with getting local time: %s", strerror(errno));
            printf("Timer failure with getting local time \n");
            break;
        }
        
        ret = strftime(output,sizeof(output),"timestamp:%a, %d %b %Y %T %z \n", buff);

        if(ret==0){
            syslog(LOG_ERR, "Failure with timer string: %s", strerror(errno));
            printf("Timer string failure \n");
            break;
        }

        printf("Timer output: %s \n", output);

        pthread_mutex_lock(e->mutex);

        serverfile = fopen(LOG_FILE, "a");
        if (serverfile < 0)
        {  
            syslog(LOG_ERR, "Failed to open file to write timer: %s \n", strerror(errno));
            printf("Failed to open file in timer \n");
            pthread_mutex_unlock(e->mutex);
            break;
        }

        //write buffer value to serverfile before resetting buffer and closing file
        fprintf(serverfile,"%s", output);
        
        fclose(serverfile);
        pthread_mutex_unlock(e->mutex);
        
    }

    return e;
}

int main(int argc, char **argv)
{
    pid_t pid;
    int ret;


    openlog("aesdsocket_server", LOG_CONS | LOG_PID, LOG_USER);

    ret = pthread_mutex_init(&mutex, NULL);
    if(ret != 0)
    {
        syslog(LOG_ERR,"mutex init failed");
        return -1;
    }

    serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_sockaddr, 0, sizeof(server_sockaddr));
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = htons(PORT);
    server_sockaddr.sin_addr.s_addr = INADDR_ANY;
    
    // if d is specified launch as daemon
    if(argc==2 && argv[1][1] == 'd') 
    {
      pid = fork();
      if (setsid() < 0)
        exit(-1);
      if (pid > 0)
        exit(0);
    
    }

    TAILQ_HEAD(head_s, node) head;
    TAILQ_INIT(&head);

    list_node * time_node = NULL;
    time_node = malloc(sizeof(list_node));
    time_node->args.sock_fd = -1;
    time_node->args.mutex = &mutex;
    time_node->args.canceled = &is_canceled;

    TAILQ_INSERT_TAIL(&head, time_node, nodes);
    pthread_create(&time_node->thread, NULL, timer, &time_node->args);

    int opt = 1;
	if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
	}

    if (bind(serv_sock, (struct sockaddr *)&server_sockaddr, sizeof(server_sockaddr)) == -1) {
        perror("bind");
        close(serv_sock);
        exit(EXIT_FAILURE);
    }

    socklen_t client_addr_len = sizeof(client_sockaddr);

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    // Listen for incoming connections
    if (listen(serv_sock, 5) == -1) {
        perror("listen");
        close(serv_sock);
        exit(EXIT_FAILURE);
    }
     


    while(!is_canceled)
    {
        int client_sock = accept(serv_sock, (struct sockaddr *)&client_sockaddr, &client_addr_len);
        if (client_sock == -1) {
            perror("accept");
            continue;
        }

        list_node * new_node = NULL;

        new_node = malloc(sizeof(list_node));
        if(new_node == NULL)
        {
            syslog(LOG_ERR,"Node malloc failed");
            return -1;
        }

        new_node->args.mutex = &mutex;
        new_node->args.canceled = &is_canceled;
        new_node->args.finished = 0;
        new_node->args.sock_fd = client_sock;

        TAILQ_INSERT_TAIL(&head, new_node, nodes);

        pthread_create(&new_node->thread, NULL, worker, &new_node->args);

        list_node * np = NULL; 

        TAILQ_FOREACH(np, &head, nodes)
        {
            if(np->args.finished)
            {
                pthread_join(np->thread, NULL);

                close(np->args.sock_fd);
                TAILQ_REMOVE(&head, np, nodes);
                free(np);
            }
        }
    }

    list_node * np = TAILQ_FIRST(&head);
    while (np != NULL)
    {
        pthread_join(np->thread, NULL);
        close(np->args.sock_fd);
        
        list_node * n_next = TAILQ_NEXT(np, nodes);
        free(np);
        np = n_next;
    }

    return 0;
}
