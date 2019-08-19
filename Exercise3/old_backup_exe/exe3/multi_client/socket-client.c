/*
 * socket-client.c
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

int main(int argc, char *argv[])
{
	int sd, port;
	int line;
	ssize_t n;
	char buf[100];
	char *hostname;
	struct hostent *hp;
	struct sockaddr_in sa;
	bool exited=false;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
		exit(1);
	}
	hostname = argv[1];
	port = atoi(argv[2]); /* Needs better error checking */

	/* Create TCP/IP socket, used as main chat channel */
	if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	fprintf(stderr, "Created TCP socket\n");
	
	/* Look up remote hostname on DNS */
	if ( !(hp = gethostbyname(hostname))) {
		printf("DNS lookup failed for host %s\n", hostname);
		exit(1);
	}

	/* Connect to remote TCP port */
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	memcpy(&sa.sin_addr.s_addr, hp->h_addr, sizeof(struct in_addr));
	fprintf(stderr, "Connecting to remote host... "); fflush(stderr);
	if (connect(sd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		perror("connect");
		exit(1);
	}
	fprintf(stderr, "Connected.\n");
	fprintf(stderr, "To end the connection press '#' alone in a new line\n");
/*-----------------------------------------------------------------------------*/
	do{
		fprintf(stdout, "\nI say:\n");
		line = getLine (buf, sizeof(buf));
		while(line!=1){
			if(line==2){
				fprintf(stdout,"Too long message. Try again.\n");
				fflush(stdout);
				fprintf(stdout, "\nI say:\n");
			}
			line = getLine(buf, sizeof(buf));		
		}	
	
	
		/* Say something... */
		if (insist_write(sd, buf, strlen(buf)) != strlen(buf)) {
			perror("write");
			exit(1);
		}
	

		/* Read answer and write it to standard output */
		n = read(sd, buf, sizeof(buf));
	
		if (n < 0) {
			perror("read");
			exit(1);
		}
	
		if (n <= 0)
			break;
		
		if(*buf == '*'){
			exited = true;
		}
		else{
			fprintf(stdout, "Server says:\n");
			fflush(stdout);

			if (insist_write(0, buf, n) != n) {
				perror("write");
				exit(1);
			}
		}

	}while(!exited);
/*----------------------------------------------------------------------------*/
	/*
	 * Let the remote know we're not going to write anything else.
	 * Try removing the shutdown() call and see what happens.
	 */
	printf("i am exiting...\n");
	if (shutdown(sd, SHUT_WR) < 0) {
		perror("shutdown");
		exit(1);
	}


	fprintf(stderr, "\nDone.\n");
	return 0;
}
