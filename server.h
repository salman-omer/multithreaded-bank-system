/*****
*
*	Define structures and function prototypes for your sorter
*
*
*
******/

#ifndef server_h
#define server_h

typedef enum { false, true } bool;

typedef struct singleClientHandlerArgs{
    int socketFd;
    struct sockaddr_storage* client_addr;
} singleClientHandlerArgs;

typedef struct tidListNode{
    pthread_t tid;
    struct tidListNode* next;
} tidListNode;

typedef struct accountNode{
    char* accountName;
    double balance;
    bool inService;
    struct accountNode* next;
} accountNode;


#endif 