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
#include <syslog.h>
#include <netinet/in.h>
#include <sys/queue.h>
#include <pthread.h>
#include <stdatomic.h>

#define LOG_FILE "/var/tmp/aesdsocketdata"
#define PORT 9000

struct sockaddr_in server_sockaddr, client_sockaddr;
FILE *fp = NULL;
int serv_sock, client_sock;
char replyMessage;

atomic_int is_canceled = 0;


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
    fclose(fp);
    close(serv_sock);
    close(client_sock);
    remove(LOG_FILE); 
    closelog();
    exit(0);
}


int main(int argc, char **argv)
{
    pid_t pid;
    char ipinput[INET_ADDRSTRLEN];

    openlog("aesdsocket_server", LOG_CONS | LOG_PID, LOG_USER);

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
     
     fp = fopen(LOG_FILE, "a+");

     ssize_t bytes_received;
     char c;


    while(1)
    {
        client_sock = accept(serv_sock, (struct sockaddr *)&client_sockaddr, &client_addr_len);
        if (client_sock == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(AF_INET, &(client_sockaddr.sin_addr), ipinput, INET_ADDRSTRLEN);

        do
        {
            bytes_received = recv(client_sock, &replyMessage, 1, MSG_WAITALL);
            if(bytes_received < 0)
            {
                perror("recv");
                exit(-1);
            }
            fprintf(fp, "%c", replyMessage);
        } while (replyMessage != '\n');
 
        rewind(fp);
        while (!feof(fp)) {
            c = fgetc(fp);
            if(feof(fp))
                break;
            send(client_sock, &c, 1, 0);
        }

        syslog(LOG_INFO, "Closed connection from %s", ipinput);
        close(client_sock);
    }

    return 0;
}
