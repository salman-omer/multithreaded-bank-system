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


#define BACKLOG 10     // how many pending connections queue will hold

#define MAXDATASIZE 100 // max number of bytes we can get at once 

typedef enum { false, true } bool;

typedef struct singleClientHandlerArgs{
    int socketFd;
    struct sockaddr_storage* client_addr;
    bool* forceExitThread;
} singleClientHandlerArgs;

typedef struct tidListNode{
    pthread_t tid;
    bool* forceExitThread;
    struct tidListNode* next;
} tidListNode;

typedef struct accountNode{
    char* name;
    double balance;
    bool inService;
    struct accountNode* next;
} accountNode;


// GLOBAL VARIABLES

tidListNode* tidList = NULL;
int sockfd;


// add the input tid to beginning of the linked list
void addTidToList(pthread_t tid, bool* forceExitThreadArg){
    if(tidList == NULL){
        tidList = malloc(sizeof(tidListNode));
        tidList->tid = tid;
        tidList->forceExitThread = forceExitThreadArg;
        tidList->next = NULL;
        return;
    }
    tidListNode* curr = tidList;
    tidListNode* newNode = malloc(sizeof(tidListNode));
    newNode->tid = tid;
    newNode->forceExitThread = forceExitThreadArg;
    newNode->next = curr;
    tidList = newNode;
    return;
}

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
    exit(1);
}

// Handler for SIGINT, caused by 
// Ctrl-C at keyboard 
void handle_sigint(int sig) 
{ 
    printf("Exit signal %d caught \n", sig); 

    // join all the tids
    tidListNode* curr = tidList;
    while(curr != NULL){
        void* status;
        *(curr->forceExitThread) = true;
        //pthread_cancel(curr->tid);
        pthread_join(curr->tid, &status);
        
        tidListNode* currCopyToFree = curr;
        curr = curr->next;
        free(currCopyToFree);
    }

    close(sockfd);
    exit(1);
} 


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void* singleClientHandler(void* args){
    singleClientHandlerArgs* argsStruct= (singleClientHandlerArgs*)args;
    char buf[MAXDATASIZE];
    int numbytes;
    char s[INET6_ADDRSTRLEN];

    inet_ntop(argsStruct->client_addr->ss_family,
        get_in_addr((struct sockaddr *)argsStruct->client_addr),
        s, sizeof(s));

    printf("server: got connection from %s\n", s);

  
    // server first recieves, then it sends back a response
    while(1){
        if(*(argsStruct->forceExitThread) == true){
            pthread_exit((void*)1);
        }

        // reset buffer
        buf[0] = '\0';

        if ((numbytes = recv(argsStruct->socketFd, buf, MAXDATASIZE-1, 0)) == -1) {
            perror("recv");
            exit(1);
        } else if (numbytes == 0) {
            printf("Connection closed by client\n");
            break;
        } else {
            // make the first bar a '\0'

            int i;
            for (i = 0; i < MAXDATASIZE - 1; ++i)
            {
                if(buf[i] == '|'){
                    buf[i] = '\0';
                }
            }

            printf("server: received '%s'\n",buf);
        }



        char *strToSend = (char*)malloc((44 + strlen(buf) + 1) * sizeof(char));
        sprintf(strToSend, "Hi client, here's what the server recieved:  %s!", buf);
        if (send(argsStruct->socketFd, strToSend, 44 + strlen(buf) + 1, 0) == -1)
            perror("send");
    }


  
    close(argsStruct->socketFd);

    return NULL;
}


// check if input port string is a valid port number
// return 0 if valid, 1 otherwise
int isValidPortNumber(char* portString){
    int portNum = atoi(portString);
    if(portNum == 0 || portNum < 8192 || portNum > 65535){
        return 1;
    }
    return 0;
}


int main(int argc, char* argv[])
{

    signal(SIGINT, handle_sigint);


    //base case test - must have 2 arguments
    if (argc != 2)
    {
        printf("FATAL ERROR: INCORRECT NUMBER OF INPUTS\n");
        write(2, "FATAL ERROR: INCORRECT NUMBER OF INPUTS\n", 41);
        return 1;
    }

    if(isValidPortNumber(argv[1]) == 1){
        printf("FATAL ERROR: INPUT PORT NUMBER IS NOT VALID\n");
        write(2, "FATAL ERROR: INPUT PORT NUMBER IS NOT VALID\n", 41);
        return 1;
    }





   
    int new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
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



    //TODO: make a thread to handle printing every 15 seconds as per project specs




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

        // create singleClientHandler data that is necessary
        singleClientHandlerArgs *args = malloc(sizeof(singleClientHandlerArgs));
        bool* forceExitThreadArg = malloc(sizeof(bool));
        *forceExitThreadArg = false;
        args->forceExitThread = forceExitThreadArg;
        args->socketFd = new_fd;
        args->client_addr = &their_addr;
        
        pthread_create(&tid, NULL, singleClientHandler, (void *)args);
        addTidToList(tid, forceExitThreadArg);

    }


    return 0;
}