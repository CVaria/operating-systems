/*
 * socket-server.c
 * Simple TCP/IP communication using sockets
 *
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "socket-common.h"

#define MAXMSG  100
#define MAX_CLN 10
/* Convert a buffer to upercase */
void toupper_buf(char *buf, size_t n)
{
	size_t i;
	
	for (i = 0; i < n; i++)
		buf[i] = toupper(buf[i]);
	
}

/*Get input from stdin with a safe way*/
static int getLine (char *buff, size_t sz) {
    int ch, extra;

    if (fgets (buff, sz, stdin) == NULL)
        return 0;

    if(buff[0]=='\n')
	return 0;
    // If it was too long, there'll be no newline. In that case, we flush
    // to end of line so that excess doesn't affect the next call.
    if (buff[strlen(buff)-1] != '\n') {
        extra = 0;
        while (((ch = getchar()) != '\n') && (ch != EOF))
            extra = 1;
        return (extra == 1) ? 2 : 1;
    }

    // Otherwise remove newline and give string back to caller.
    /* Be careful with buffer overruns, ensure NUL-termination */
    buff[strlen(buff)-1] = '\0';
    return 1;
}

/* Insist until all of the data has been written */
ssize_t insist_write(int fd, const void *buf, size_t cnt)
{
	ssize_t ret;
	size_t orig_cnt = cnt;
	
	while (cnt > 0) {
	        ret = write(fd, buf, cnt);
	        if (ret < 0)
	                return ret;
	        buf += ret;
	        cnt -= ret;
	}

	return orig_cnt;
}

/*Read data from some client and check if client is exiting*/

int read_and_reply(int newsd, int client) {
	
	int n;
	char buff[MAXMSG];

	n = read(newsd, buff, sizeof(buff));
	
	if (n <0){
		perror("read from remote peer failed");
		return -1;
	}	
	else if (n==0){
		fprintf(stderr, "Peer went away\n");
		return 0;
	}
	
	if(*buff == '#'){
		fprintf(stdout, "client %d with socket %d is exiting...\n", client, newsd);
		fflush(stdout);
		buff[0] = '*';
		buff[1] = '\0';
		return -2;
	} 	
	printf("Server got message from client %d with socket %d:\n",client,newsd);
	if(insist_write(0,buff,n)!=n){
		perror("write");
		return -1;
	}

	fprintf(stdout,"\n");
	fflush(stdout);

	toupper_buf(buff, n);

	/* Say something... */
	if (insist_write(newsd, buff, n) != n) {
		perror("write");
		return -1;
	}	

	return n;				
    
}

int main(void)
{
	//char buf[100];
	char addrstr[INET_ADDRSTRLEN];
	int client_sockets[MAX_CLN],server_sock, newsd, max_sd, temp_sock;
	//int line;
	ssize_t n;
	socklen_t len;
	struct sockaddr_in sa;
	fd_set active_fd_set, read_fd_set;
  	int i, j;
	//bool exited=false;
	
	/* Make sure a broken connection doesn't kill us */
	signal(SIGPIPE, SIG_IGN);

	/* Create TCP/IP socket, used as main chat channel */
	if ((server_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	fprintf(stderr, "Created TCP socket\n");

	/* Bind to a well-known port */
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(TCP_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(server_sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		perror("bind");
		exit(1);
	}
	fprintf(stderr, "Bound TCP socket to port %d\n", TCP_PORT);

	/* Listen for incoming connections */
	if (listen(server_sock, TCP_BACKLOG) < 0) {
		perror("listen");
		exit(1);
	}

	for(i=0; i<MAX_CLN; i++){
		client_sockets[i]=0;
	}
	client_sockets[0]=server_sock;
	/*max_sd is used to keep the maximum value in sockets' set.*/
	/*This way we dont need to examine every possible socket (from 0 to FD_SETSIZE)*/
	max_sd=server_sock;

	 /* Initialize the set of active sockets. */
	 FD_ZERO (&active_fd_set);
	 FD_SET (server_sock, &active_fd_set);

	/* Loop forever, accept()ing connections */
	while(1) {

		read_fd_set = active_fd_set;
		if(select(max_sd+1, &read_fd_set,NULL, NULL, NULL)<0){
		          perror ("select");
		          exit(1);
	        }
		//printf("here again\n");

		/*There was a request. Find which socket's got data*/
		for(i=0; i<MAX_CLN; i++){
			temp_sock=client_sockets[i];
			if(FD_ISSET(temp_sock, &read_fd_set)){
				//printf("max_sd=%d\n", max_sd);
				if(temp_sock==server_sock){
					fprintf(stderr, "There is an incoming connection...\n");

					/* Accept an incoming connection */
					len = sizeof(struct sockaddr_in);
					if ((newsd = accept(server_sock, (struct sockaddr *)&sa, &len)) < 0) {
						perror("accept");
						exit(1);
					}
					if (!inet_ntop(AF_INET, &sa.sin_addr, addrstr, sizeof(addrstr))) {
						perror("could not format IP address");
						exit(1);
					}
					fprintf(stderr, "Incoming connection from %s:%d\n",
						addrstr, ntohs(sa.sin_port));

					/*Update table of clients' sockets*/
					for(j=0; j<MAX_CLN; j++){		
						if(client_sockets[j]==0){
							client_sockets[j]=newsd;
							break;
						}
					}
					/*Update max socket value*/
					if(newsd>max_sd)
						max_sd=newsd;

					printf("adding new client\n");
					FD_SET(newsd, &active_fd_set);
					//printf("max_sd=%d after adding\n", max_sd);

				}
				else{		
					/*If pending request is not from server socket, some client wrote something*/
					n = read_and_reply(temp_sock,i);
					/*Check if there was some error or client wants to disconnect*/
					if(n<0){
						/* Make sure we don't leak open files */
						if(close(temp_sock)<0)	
							perror("close");
						FD_CLR(temp_sock, &active_fd_set);

						client_sockets[i]=0;
						/*Remove socket from set and update max_sd*/
						if(temp_sock== max_sd){
							max_sd=0;
							for(j=0; j<MAX_CLN; j++){		
								if(client_sockets[j]>max_sd)
									max_sd=client_sockets[j];
							}
						}
						//printf("max_sd=%d after deleting\n", max_sd);			
					}
				}
			}
		}
	}
	/* This will never happen */
	return 1;
}

