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

int main(void)
{
	char buf[100];
	char addrstr[INET_ADDRSTRLEN];
	int sd, newsd;
	int line;
	ssize_t n;
	socklen_t len;
	struct sockaddr_in sa;
	bool exited=false;
	
	/* Make sure a broken connection doesn't kill us */
	signal(SIGPIPE, SIG_IGN);

	/* Create TCP/IP socket, used as main chat channel */
	if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	fprintf(stderr, "Created TCP socket\n");

	/* Bind to a well-known port */
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(TCP_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		perror("bind");
		exit(1);
	}
	fprintf(stderr, "Bound TCP socket to port %d\n", TCP_PORT);

	/* Listen for incoming connections */
	if (listen(sd, TCP_BACKLOG) < 0) {
		perror("listen");
		exit(1);
	}

	/* Loop forever, accept()ing connections */
	for (;;) {
		fprintf(stderr, "Waiting for an incoming connection...\n");

		/* Accept an incoming connection */
		len = sizeof(struct sockaddr_in);
		if ((newsd = accept(sd, (struct sockaddr *)&sa, &len)) < 0) {
			perror("accept");
			exit(1);
		}
		if (!inet_ntop(AF_INET, &sa.sin_addr, addrstr, sizeof(addrstr))) {
			perror("could not format IP address");
			exit(1);
		}
		fprintf(stderr, "Incoming connection from %s:%d\n",
			addrstr, ntohs(sa.sin_port));

		exited=false;
/*-----------------------------------------------------------------------------------------------*/		
		do{
			n = read(newsd, buf, sizeof(buf));
			if (n <= 0) {
				if (n < 0)
					perror("read from remote peer failed");
				else
					fprintf(stderr, "Peer went away\n");
				break;
			}
			
			if(*buf == '#'){
				fprintf(stdout, "client is exiting...\n");
				fflush(stdout);
				exited = true;
				buf[0] = '*';
				buf[1] = '\0';
			} 				
			else{	
				fprintf(stdout, "client says:\n");
				fflush(stdout);			
			
				if (insist_write(0, buf, n) != n) {
				perror("write");
				exit(1);
				}
			
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

			}
			/* Say something... */
			if (insist_write(newsd, buf, strlen(buf)) != strlen(buf)) {
				perror("write");
				exit(1);
			}


		}while(!exited);
/*-----------------------------------------------------------------------------------------------*/
		
		/* Make sure we don't leak open files */
		if (close(newsd) < 0)
			perror("close");
	}

	/* This will never happen */
	return 1;
}

