#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

//#define PORT "10000"
#define MAXINPUTSIZE 300


int sockfd;

// Handler for SIGINT, caused by 
// Ctrl-C at keyboard 
void handle_sigint(int sig) 
{ 
    printf("Exiting...\n"); 
    // close(sockfd);
    printf("Client has disconnected from the server.\n");
    exit(1);
}

void *threadSend(void * send_arg)
{
    int byteSize;
    sockfd = *((int *)send_arg);

    while (1)
    {
        // char buf[MAXINPUTSIZE];
        char cmd[10], accountName[256];
        double amount;
        char msg[MAXINPUTSIZE];
        

        sleep(2);   //throttling for 2 seconds

        fgets(msg, 10000, stdin);

        // sleep(2);
        // printf("Message is: %s\n", msg);

        // printf("Message length is: %i\n", strlen(msg));
        msg[strlen(msg)-1] = '\0';

        int index = 0, i = 0;

        /*
        while (msg[index] == ' ' || msg[index] == '\t')     //checking leading spaces
        {
            index++;
        }
        */

        //first get till space then cut 
        while (msg[i] != ' ' && i < 9 && msg[i] != '\0' && msg[i] != '\t')       //get command
        {
            // printf("msg: %c\n", msg[index + i]);
            cmd[i] = msg[i];
            i++;
        }
        cmd[i] = '\0';  //leading spaces removed + cmd now has the command syntax
        // printf("Command is: %s\n", cmd);

        //check if invalid command
        if ((strcmp(cmd, "create") != 0) && (strcmp(cmd, "serve") != 0) && (strcmp(cmd, "deposit") != 0) && (strcmp(cmd, "withdraw") != 0) && (strcmp(cmd, "query") != 0) && (strcmp(cmd, "end") != 0) && (strcmp(cmd, "quit") != 0))
        {
            printf("ERROR: INVALID COMMAND\n");
            write(2, "ERROR: INVALID COMMAND\n", 24);
            printf("\n");
            continue;
        }
        // printf("Flag: ONLY VALID COMMAND MAY PASS!!!\n");

        index = i + 1;
        i = 0;

        
        if ((strcmp(cmd, "create") == 0) || (strcmp(cmd, "serve") == 0))                //create & serve
        {
            
            /*
            while (msg[index] == ' ' || msg[index] == '\t')     //removing unnecessary spaces in between command and second input
            {
                index++;
            }
            */

            while (msg[index + i] != '\0')       //get second input
            {
                accountName[i] = msg[index + i];
                i++;
            }

            if(i > 255){
                printf("ERROR: ACCOUNT NAME IS TOO LONG\n");
                write(2, "ERROR: ACCOUNT NAME IS TOO LONG\n", 33);
                printf("\n");
                continue;
            }


            accountName[i] = '\0';  //leading spaces removed + accountName now acquired
            // printf("accountName is: %s\n", accountName);

            if (strlen(accountName) < 1)
            {
                printf("ERROR: INVALID ACCOUNT NAME\n");
                write(2, "ERROR: INVALID ACCOUNT NAME\n", 29);
                printf("\n");
                continue;
            }

            //send to server
            char finalMsg[strlen(cmd)+strlen(accountName)+2];
            snprintf(finalMsg, sizeof finalMsg + 1, "%s %s|", cmd, accountName);
            if ((byteSize = send(sockfd, finalMsg, strlen(finalMsg), 0)) == -1)
            {
                // printf("U here?.\n");
                perror("send");
                exit(1);
            }
            // printf("Client has sent '%s' command to the server.\n\n", finalMsg);
            
            //recv from server
            /*
            if ((byteSize = recv(sockfd, buf, MAXINPUTSIZE - 1, 0)) == -1)
            {
                perror("recv");
                return 1;
            }
            buf[byteSize] = '\0';
            printf("Client received '%s'\n", buf);
            */
            
            continue;

        }
        else if ((strcmp(cmd, "deposit") == 0) || (strcmp(cmd, "withdraw") == 0))       //deposit & withdraw
        {
            while (msg[index + i] != ' ' && msg[index + i] != '\0')       //get second input
            {
                accountName[i] = msg[index + i];
                i++;
            }
            accountName[i] = '\0';
            // printf("soon to be amount is: %s\n", accountName);

            if (msg[index + i + 1] != '\0')
            {
                printf("ERROR: INVALID FORMAT\n");
                write(2, "ERROR: INVALID FORMAT\n", 23);
                printf("\n");
                continue;
            }

            amount = atof(accountName);
            if ((amount == 0 && accountName[0] != '0') || amount < 0)
            {
                printf("ERROR: INVALID AMOUNT FORMAT\n");
                write(2, "ERROR: INVALID AMOUNT FORMAT\n", 30);
                printf("\n");
                continue;
            }
            // printf("Amount is: %f\n", amount);


            //send to server
            char finalMsg[strlen(cmd)+strlen(accountName)+2];
            snprintf(finalMsg, sizeof finalMsg + 1, "%s %s|", cmd, accountName);
            if ((byteSize = send(sockfd, finalMsg, strlen(finalMsg), 0)) == -1)
            {
                // printf("U here?.\n");
                perror("send");
                exit(1);
            }
            // printf("Client has sent '%s' command to the server.\n\n", finalMsg);
            
            //recv from server
            /*
            if ((byteSize = recv(sockfd, buf, MAXINPUTSIZE - 1, 0)) == -1)
            {
                perror("recv");
                return 1;
            }
            buf[byteSize] = '\0';
            printf("Client received '%s'\n", buf);
            */
            
            continue;

        }
        else                                                                        //send query, end, quit command to server
        {
            if (msg[strlen(cmd)] != '\0')
            {
                printf("ERROR: INVALID FORMAT\n");
                write(2, "ERROR: INVALID FORMAT\n", 23);
                printf("\n");
                continue;
            }
            // printf("U here?2\n");
            char finalMsg[strlen(cmd)+1];
            snprintf(finalMsg, sizeof finalMsg + 1, "%s|", cmd);
            if ((byteSize = send(sockfd, finalMsg, strlen(finalMsg), 0)) == -1)
            {
                // printf("U here?.\n");
                perror("send");
                exit(1);
            }
            // printf("Client has sent '%s' command to the server.\n\n", finalMsg);
            

            //recv from server
            /*
            if ((byteSize = recv(sockfd, buf, MAXINPUTSIZE - 1, 0)) == -1)
            {
                perror("recv");
                return 1;
            }
            buf[byteSize] = '\0';
            printf("Client received '%s'\n", buf);
            */
            
            continue;
        }
        
    }
    
    return NULL;
}

void *threadRecv(void * recv_arg)
{
    int byteSize;
    sockfd = *((int *)recv_arg);

    while (1)
    {
        char buf[MAXINPUTSIZE];

        if ((byteSize = recv(sockfd, buf, MAXINPUTSIZE - 1, 0)) == -1)
        {
            perror("recv");
            exit(1);
        }
        else if (byteSize == 0)
        {
            printf("Connection closed by server.\n");
            close(sockfd);
            printf("Client has successfully disconnected from the server.\n");
            exit(1);
        }
        buf[byteSize] = '\0';
        printf("Client received '%s'\n\n", buf);
    }
    
    return NULL;
}

int main(int argc, char *argv[])
{
    int status;
    struct addrinfo hints, *res, *p;
    char strIPaddr[INET_ADDRSTRLEN];    //IPv4 addr


    signal(SIGINT, handle_sigint);

    // int new_sockfd;
    // struct sockaddr_storage their_addr;
    // socklen_t addr_size;

    //base case test - must have 3 arguments
    if (argc != 3)
    {
        printf("FATAL ERROR: INCORRECT NUMBER OF INPUTS\n");
  		write(2, "FATAL ERROR: INCORRECT NUMBER OF INPUTS\n", 41);
        printf("\n");
  		return 1;
    }
    //Input ex: ./bankingClient cp.cs.rutgers.edu 9999

    memset(&hints, 0, sizeof hints);    //make sure struct is empty
    hints.ai_family = AF_INET;          //IPv4
    hints.ai_socktype = SOCK_STREAM;    //TCP socket
    
    if ((status = getaddrinfo(argv[1], argv[2], &hints, &res)) != 0)
    {
        printf("getaddrinfo ERROR: %s\n", gai_strerror(status));
  		//write(2, ("getaddrinfo ERROR: %s\n", gai_strerror(status)), 40);
        fprintf(stderr, "getaddrinfo ERROR: %s\n", gai_strerror(status));
  		return 1;
    }

/*
    //put this in while loop with 3 second timer so client can be turned on first and still connect to server after
    //loop thru all res and connect to first one
    for (p = res; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)  //socket error
        {
            perror("Client: Socket");
            continue;
        }

        if ((connect(sockfd, p->ai_addr, p->ai_addrlen)) == -1)     //connect error
        {
            perror("Client: Connect");
            continue;
        }
        break;
    }

    //Check if client failed to connect
    if (p == NULL)
    {
        printf("FATAL ERROR: CLIENT FAILED TO CONNECT\n");
  		write(2, "FATAL ERROR: CLIENT FAILED TO CONNECT\n", 39);
  		return 1;
    }
*/

    //client or server can be provoked in any order
    while (1)
    {
        for (p = res; p != NULL; p = p->ai_next)
        {
            if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)  //socket error
            {
                perror("Client: Socket");
                sleep(3);
                continue;
            }

            if ((connect(sockfd, p->ai_addr, p->ai_addrlen)) == -1)     //connect error
            {
                perror("Client: Connect");
                sleep(3);
                continue;
            }
            break;
        }
        if (p != NULL)
        {
            break;
        }
    }


    inet_ntop(p->ai_family, p->ai_addr, strIPaddr, sizeof strIPaddr);
    printf("Client connecting to %s\n", strIPaddr);

    freeaddrinfo(res);      //no longer needed and cleared

    /*
    if ((byteSize = recv(sockfd, buf, MAXINPUTSIZE - 1, 0)) == -1)
    {
        perror("recv");
        return 1;
    }

	buf[byteSize] = '\0';
    printf("Client has successfully connected to the server.\n");
	printf("Client received '%s'\n", buf);
    */
    printf("Client has successfully connected to the server.\n\n");


    // struct timeval tv;
    // fd_set readfds;

    // tv.tv_sec = 2;
    // tv.tv_usec = 0;

    // FD_ZERO(&readfds);
    // FD_SET(STDIN, &readfds);

    /* command prompt */    
    pthread_t send_tid, recv_tid;

    pthread_create(&send_tid, NULL, threadSend, (void *)(&sockfd));
    pthread_create(&recv_tid, NULL, threadRecv, (void *)(&sockfd));

    void* tid_status;
    pthread_join(send_tid,&tid_status);
    pthread_join(recv_tid,&tid_status);
    
    pthread_cancel(send_tid);
    pthread_cancel(recv_tid);

    // close(sockfd);
    // printf("Client has successfully disconnected from the server.\n");

    return 0;
}
