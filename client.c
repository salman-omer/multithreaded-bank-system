#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//#define PORT "10000"
#define MAXINPUTSIZE 300

int main(int argc, char *argv[])
{
    int sockfd, status, byteSize;
    struct addrinfo hints, *res, *p;
    char strIPaddr[INET_ADDRSTRLEN];    //IPv4 addr
    char buf[MAXINPUTSIZE];

    //base case test - must have 3 arguments
    if (argc != 3)
    {
        printf("FATAL ERROR: INCORRECT NUMBER OF INPUTS\n");
  		write(2, "FATAL ERROR: INCORRECT NUMBER OF INPUTS\n", 41);
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

    inet_ntop(p->ai_family, p->ai_addr, strIPaddr, sizeof strIPaddr);
    printf("Client connecting to %s ...\n", strIPaddr);

    freeaddrinfo(res);      //no longer needed and cleared


    if ((byteSize = recv(sockfd, buf, MAXINPUTSIZE - 1, 0)) == -1)
    {
        perror("recv");
        return 1;
    }

	buf[byteSize] = '\0';
	printf("Client received '%s'\n", buf);

    close(sockfd);

    return 0;
}
