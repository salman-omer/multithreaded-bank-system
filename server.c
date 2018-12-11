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
#include <semaphore.h>
#include <sys/time.h>
#include "server.h"


#define BACKLOG 10     // how many pending connections queue will hold

#define MAXDATASIZE 100 // max number of bytes we can get at once 




// GLOBAL VARIABLES
accountNode* accountsList = NULL;
tidListNode* tidList = NULL;
int sockfd;
pthread_mutex_t createAccountLock;
sem_t printLock;




// add the input tid to beginning of the linked list
void addTidToList(pthread_t tid){
    if(tidList == NULL){
        tidList = malloc(sizeof(tidListNode));
        tidList->tid = tid;
        tidList->next = NULL;
        return;
    }
    tidListNode* curr = tidList;
    tidListNode* newNode = malloc(sizeof(tidListNode));
    newNode->tid = tid;
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
    printf("Exiting...\n"); 

    // join all the tids
    tidListNode* curr = tidList;
    while(curr != NULL){
        pthread_cancel(curr->tid);
        
        tidListNode* currCopyToFree = curr;
        curr = curr->next;
        free(currCopyToFree);
    }

    close(sockfd);
    sem_destroy(&printLock);
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

//return the command type for the input command
//returns an int with the following correspondence:
//1 ="create";  2  = "serve"; 3 = "deposit", 4 = "withdraw", 5 ="query",  6 ="end",  7 ="quit",   8 =  "invalid command"
int getCommandType(char* inputCommand){
    if(inputCommand == NULL || strlen(inputCommand)  < 3){
        return 8;
    }

    //create
    if(inputCommand[0] == 'c'){ 
        return 1;
    }

    //quit or query
    if(inputCommand[0] == 'q'){

        // quit
        if(inputCommand[2] == 'i'){
            return 7;
        }

        //query
        if (inputCommand[2] == 'e')
        {
            return 5;
        }
    }

    if(inputCommand[0] == 's'){
        return 2;
    }
    if(inputCommand[0] == 'd'){
        return 3;
    }
    if(inputCommand[0] == 'w'){
        return 4;
    }

    if(inputCommand[0] == 'e'){
        return 6;
    }

    return 8;
}

// return accountNode for the input name from the master list, return NULL if not found
accountNode* getAccount(char* name){
    accountNode* curr = accountsList;
    while(curr != NULL){
        if(strcmp(curr->accountName, name) == 0){
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

// takes the input string and creates an account with the appropriate name
// adds that account node to the list of account nodes
// arg: create command of format "create <accountName>"
// ret: a new account node for that account, NULL if it could not be created
accountNode* createAccount(char* command){


    pthread_mutex_lock(&createAccountLock); 

    char* accountName = malloc(sizeof(char) * (strlen(&command[7]) + 1));
    strcpy(accountName, &(command[7]));

    // if account of that name already exists, return NULL
    if(getAccount(accountName) != NULL){
        //printf("Account %s already exists\n", accountName);
        pthread_mutex_unlock(&createAccountLock);
        return NULL;
    }else{  // need to make new account if it doesnt exist
        accountNode* newAccount = malloc(sizeof(accountNode));
        accountNode* temp = accountsList;
        newAccount->accountName = accountName;
        newAccount->balance = 0;
        newAccount->inService = false;
        newAccount->next = temp;

        sem_wait(&printLock);
        accountsList = newAccount;
        sem_post(&printLock);

        //printf("New Account Created for %s\n", accountName);
        pthread_mutex_unlock(&createAccountLock);
        return newAccount;
    }

    pthread_mutex_unlock(&createAccountLock); 

    return NULL;
}


// takes input command and returns account to serve
// checks if the account is in service
// arg: serve command of format 'serve <accountName>'
// ret: accountNode to serve, null if does not exist OR if it is in service
accountNode* serveAccount(char* command){
    char* accountName = malloc(sizeof(char) * (strlen(&command[6]) + 1));
    strcpy(accountName, &(command[6]));
    accountNode* serviceAccount = getAccount(accountName);
    if(serviceAccount == NULL || serviceAccount->inService == true){
        return NULL;
    }

    sem_wait(&printLock);
    serviceAccount->inService = true;
    sem_post(&printLock);
    return serviceAccount;
}


// takes input deposit command and adds money to input bank account
// arg: deposit command of format 'deposit <amount>', accountNode to deposit to
// ret: balance of bank account, -1 if acc doesnt exist
double deposit(char* command, accountNode* account){
    if(account == NULL){
        return -1;
    }

    double depositVal = atof(&(command[8]));
    sem_wait(&printLock);
    account->balance = account->balance + depositVal;
    sem_post(&printLock);

    return account->balance;

}


// takes input withdraw command and removes money to input bank account
// arg: deposit command of format 'withdraw <amount>', accountNode to withdraw from
// ret: balance of bank account, -1 if balance is too low to withdraw
double withdraw(char* command, accountNode* account){
    double withdrawVal = atof(&(command[9]));
    if(account->balance <  withdrawVal){
        return -1;
    }

    sem_wait(&printLock);
    account->balance = account->balance - withdrawVal;
    sem_post(&printLock);

    return account->balance;

}

//arg: account to deservice
//ret: 0 is success, 1 if already deserviced or acc is null
int deserviceAccount(accountNode* acc){
    if(acc == NULL || acc->inService == false){
        return 1;
    }
    sem_wait(&printLock);
    acc->inService = false;
    sem_post(&printLock);
    return 0;

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

      
    accountNode* currAccount = NULL;
    char* retString;
    // server first recieves, then it sends back a response
    while(1){


        // reset buffer
        buf[0] = '\0';
        retString = NULL;
        bool quit = false;

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

            // now we do something with the recieved data based on what command is submitted
            int commandType = getCommandType(buf);




            //create
            if(commandType == 1){
                //printf("server: received '%s' of command type '%s'\n",buf, "create");
                if(currAccount != NULL){
                        retString = "Error: This client is currently servicing an account, unable to make a new account";
                } else {
                    if(createAccount(buf) == NULL){
                        retString = "Error: Account name already exists in bank";
                    } else {
                        retString = "Account successfully created";
                    }
                }


            } else if (commandType == 2){ // serve
                //printf("server: received '%s' of command type '%s'\n",buf, "serve");
                if(currAccount != NULL){
                    retString = "You are currently servicing an account, 'serve' command not available";

                } else{
                    currAccount = serveAccount(buf);
                    if(currAccount == NULL){
                        retString = "Error: Account is already in service by another client or it does not exist";
                    } else {
                        retString = "Account is now being serviced";
                    }
                }

            } else if (commandType == 3){ // deposit
                //printf("server: received '%s' of command type '%s'\n",buf, "deposit");
                if(currAccount == NULL){
                    retString = "Error: No account is currently being serviced by this client, cannot deposit";
                } else {
                    retString = (char*)malloc(100 * sizeof(char));
                    //char *strToSend = (char*)malloc((44 + strlen(buf) + 1) * sizeof(char));
                    double accountBal = deposit(buf, currAccount);
                    if(accountBal < 0){
                        sprintf(retString, "Deposit unsuccessful, try again");
                    } else {
                        sprintf(retString, "Deposit successful, current account balance is %f!", accountBal);
                    }
                }




            } else if (commandType == 4){ // withdraw
                //printf("server: received '%s' of command type '%s'\n",buf, "withdraw");
                if(currAccount == NULL){
                    retString = "Error: No account is currently being serviced by this client, cannot withdraw";
                } else {
                    retString = (char*)malloc(100 * sizeof(char));
                    //char *strToSend = (char*)malloc((44 + strlen(buf) + 1) * sizeof(char));
                    double accountBal = withdraw(buf, currAccount);
                    if(accountBal < 0){
                        sprintf(retString, "Withdraw unsuccessful, balance is too low");
                    } else {
                        sprintf(retString, "Withdraw successful, current account balance is %f!", accountBal);
                    }
                }





            } else if (commandType == 5){ // query
                //printf("server: received '%s' of command type '%s'\n",buf, "query");
                if(currAccount == NULL){
                    retString = "Error: No account is currently being serviced by this client, cannot query";
                } else {
                    retString = (char*)malloc(100 * sizeof(char));
                    sprintf(retString, "Query successful, current account balance is %f!", currAccount->balance);
                }  
            } else if (commandType == 6){ // end
                //printf("server: received '%s' of command type '%s'\n",buf, "end");
                if(currAccount == NULL){
                    retString = "Error: this client is not currently servicing an account";
                } else {
                    deserviceAccount(currAccount);
                    currAccount = NULL;
                    retString = "Service session successfully ended";
                }
            } else if (commandType == 7){ // quit
                //printf("server: received '%s' of command type '%s'\n",buf, "quit");
                deserviceAccount(currAccount);
                retString = "Client disconnected from server";
                printf("Connection closed by client\n");
                quit = true;
            } else { // invalid command
                //printf("server: received '%s' of command type '%s'\n",buf, "invalid command");
                retString = "INVALID CLIENT COMMAND!";
            }

        }



        //char *strToSend = (char*)malloc((44 + strlen(buf) + 1) * sizeof(char));
        //sprintf(strToSend, "Hi client, here's what the server recieved:  %s!", buf);
        if (send(argsStruct->socketFd, retString, strlen(retString), 0) == -1)
            perror("send");

        if(quit == true){
            break;
        }
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

// prints all account information
void printDiagnosticInformation(){
    if(accountsList == NULL){
        printf("No accounts in bank\n");
        return;
    }
    printf("\n");
    accountNode* currAccount = accountsList;
    while(currAccount != NULL){
        char* inService;
        if(currAccount->inService == true){
            inService = "IN SERVICE";
        } else {
            inService = "";
        }
        printf("%s\t%f\t%s\n",currAccount->accountName,currAccount->balance,inService);
        currAccount = currAccount->next;
    }

    return;
}


void hangleSigAlarm(void) {
    sem_wait(&printLock);
    printDiagnosticInformation();
    sem_post(&printLock);
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

    sem_init(&printLock,0,1);

    struct itimerval it_val;  /* for setting itimer */
    int interval = 15000;

    /* Upon SIGALRM, call DoStuff().
    * Set interval timer.  We want frequency in ms, 
    * but the setitimer call needs seconds and useconds. */
    if (signal(SIGALRM, (void (*)(int)) hangleSigAlarm) == SIG_ERR) {
        perror("Unable to catch SIGALRM");
        exit(1);
    }
    it_val.it_value.tv_sec = interval/1000;
    it_val.it_value.tv_usec = (interval*1000) % 1000000;   
    it_val.it_interval = it_val.it_value;
    if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
        perror("error calling setitimer()");
        exit(1);
    }




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
        args->socketFd = new_fd;
        args->client_addr = &their_addr;
        
        pthread_create(&tid, NULL, singleClientHandler, (void *)args);
        addTidToList(tid);

    }


    return 0;
}