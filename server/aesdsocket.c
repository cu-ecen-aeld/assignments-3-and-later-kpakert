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

#define LOG_FILE "/var/tmp/aesdsocketdata"
#define PORT 9000

struct sockaddr_in server_sockaddr, client_sockaddr;
FILE *fp = NULL;
int serv_sock, client_sock;

void sigint_handler(int signum) {
    syslog(LOG_INFO, "Caught signal, exiting");
    fclose(fp);
    close(serv_sock);
    close(client_sock);
    unlink(LOG_FILE); 
    closelog();
    exit(0);
}


int main(int argc, char **argv)
{
    pid_t pid;



    // if d is specified launch as daemon
    if(argc==2 && argv[1][1] == 'd') 
    {
      pid = fork();
      if (setsid() < 0)
        exit(-1);
      if (pid > 0)
        exit(0);
    }


    return 0;
}
