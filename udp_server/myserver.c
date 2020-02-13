#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket() and bind() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <pthread.h>   
#include <errno.h>     
#include <string.h>    
#include <fcntl.h>    
#include <sys/wait.h>    
#include <sys/resource.h>    
#include <sys/mman.h>    
#include <signal.h>    

#define TX_BUFF_SIZE 8*1024
#define RX_BUFF_SIZE 8*1024
#define SERV_PORT 50707

struct timeval tv;
time_t sec_begin, sec_end, sec_elapsed;

typedef struct
{
	int sock;
	int stream_active;
	long long sent_count;
	long long recv_count;
	struct sockaddr_in ServAddr;
	struct sockaddr_in ClntAddr;

} handler;

static handler* phandler;

void DieWithError(char *errorMessage)
{
	perror(errorMessage);  
	exit(1);
}

void* stream_to_client(void* arg)
{
	char Buffer[TX_BUFF_SIZE];				/* Buffer for echo string */
	memset(&Buffer,0,sizeof(Buffer) );

	printf("Streaming to the client, TX buffer size: %d\n", TX_BUFF_SIZE);

	while(phandler->stream_active)
	{
		int sendMsgSize = sendto(phandler->sock,Buffer, TX_BUFF_SIZE, 0,(struct sockaddr*) &(phandler->ClntAddr), sizeof(phandler->ClntAddr));

	        if (sendMsgSize<0)
			DieWithError("Send msg failed");

		phandler->sent_count+=sendMsgSize;
	}
	printf("Sent %lld MB\n",phandler->sent_count/1024/1024);
	
	return NULL;
}


int main(int argc, char *argv[])
{
	char Buffer[RX_BUFF_SIZE];				/* Buffer for echo string */
	int recvMsgSize;		    		/* Size of received message */
	
	struct sockaddr_in ServAddr;
	memset(&ServAddr, 0, sizeof(ServAddr));
	ServAddr.sin_family = AF_INET;                	/* Internet address family */
	ServAddr.sin_addr.s_addr = htonl(INADDR_ANY); 	/* Any incoming interface */
	ServAddr.sin_port = htons(SERV_PORT);	  	/* Local port */
	
	phandler = malloc(sizeof(handler));
	phandler->stream_active = 1;
	phandler->recv_count = 0;
	phandler->sent_count = 0;
	
	memset(&(phandler->ClntAddr), 0, sizeof(phandler->ClntAddr));

        printf("Waiting for udp messages...\n");
	/* Create socket for receiving datagrams */
	if ((phandler->sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	        DieWithError("socket() failed");
		
	/* Set the socket as reusable */
	int true_v = 1;
	if (setsockopt(phandler->sock, SOL_SOCKET, SO_REUSEADDR, &true_v, sizeof (int))!=0) 
		DieWithError("Error");
		
	/* Bind to the local address */
	if (bind(phandler->sock, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) < 0)
		DieWithError("bind() failed");
		
	/* Block until receive message from a client */
	unsigned int cliAddrLen = sizeof(phandler->ClntAddr);
	if ((recvMsgSize = recvfrom(phandler->sock, Buffer, RX_BUFF_SIZE, 0,(struct sockaddr *) &(phandler->ClntAddr), &cliAddrLen)) < 0)
               	DieWithError("recvfrom() failed");
	
	phandler->recv_count += recvMsgSize;
		
	printf("Handling client %s, RX message size: %d bytes\n", inet_ntoa(phandler->ClntAddr.sin_addr),recvMsgSize);
		
	/* Set waiting limit */
	tv.tv_sec = 1;					/* Set the timeout for recv/recvfrom*/
	if (setsockopt(phandler->sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0)
		DieWithError("Error");

	/* Start streaming */
	pthread_t stream_thread;
	if(pthread_create(&stream_thread, NULL, &stream_to_client,NULL))
                DieWithError("starting thread failed");
	
	/*Reciving*/
	while(1)
	{
		recvMsgSize = recv(phandler->sock, Buffer, RX_BUFF_SIZE, 0);
		if(errno==EAGAIN)
		{ 
			setsockopt(phandler->sock, SOL_SOCKET, 0,&tv,sizeof(tv));
		    	printf("Receive timeout is reached\n");
			phandler->stream_active = 0;
			break;
		}
		if(recvMsgSize<0)
			DieWithError("Reciving failed\n");
			
		phandler->recv_count += recvMsgSize;
	}

	pthread_join(stream_thread, NULL);
	printf("Received %lld MB in total\n",phandler->recv_count/1024/1024);
	close(phandler->sock);
	free(phandler);
	
	return 0;
}

