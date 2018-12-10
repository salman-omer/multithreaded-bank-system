/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#define PORT "10000"  // the port users will be connecting to

#define BACKLOG 10     // how many pending connections queue will hold

#define MAXDATASIZE 100 // max number of bytes we can get at once 

typedef struct singleClientHandlerArgs{
    int socketFd;
    struct sockaddr_storage* client_addr;
} singleClientHandlerArgs;

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void* singleClientHander(void* args){
    singleClientHandlerArgs* argsStruct= (singleClientHandlerArgs*)args;
    char buf[MAXDATASIZE];
    int numbytes;
    char s[INET6_ADDRSTRLEN];

    inet_ntop(argsStruct->client_addr->ss_family,
        get_in_addr((struct sockaddr *)argsStruct->client_addr),
        s, sizeof(s));

    printf("server: got connection from %s\n", s);

  

    // the actual sending and recieving

    if (send(argsStruct->socketFd, "Hello, world!\n", 14, 0) == -1)
        perror("send");
    
    sleep(1);
    
    if (send(argsStruct->socketFd, "Hello, world!\n", 14, 0) == -1)
        perror("send");

    printf("Data Sent\n");

    if ((numbytes = recv(argsStruct->socketFd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    } else if (numbytes == 0) {
        printf("Connection closed by client\n");
    } else {
        printf("server: received '%s'\n",buf);
    }

  
    close(argsStruct->socketFd);

    return NULL;
}


int main(void)
{
    char buf[MAXDATASIZE];
    int sockfd, new_fd, numbytes;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        // create socket file descriptor to read from/to later
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }


        // if it thinks the socket is already in use, still proceed
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        // binds socket to port passed in with address info
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }



        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    // this server starts listening on this socket, waiting for a connection
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;

        // this accept creates a new socket file descriptor for this connection specifically
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }


        // if there is a created socket, make a thread for this interaction
        pthread_t tid;
        void* status;

        // create singleClientHandler data that is necessary
        singleClientHandlerArgs *args = malloc(sizeof(singleClientHandlerArgs));
        args->socketFd = new_fd;
        args->client_addr = &their_addr;
        
        pthread_create(&tid, NULL, singleClientHander, (void *)args);
        pthread_join(tid, &status);

    }

    close(sockfd);

    return 0;
}