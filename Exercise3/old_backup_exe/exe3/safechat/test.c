#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "socket-common.h"



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
	char *buf,*hostname,addrstr[INET_ADDRSTRLEN];;
	socklen_t len;
	int sd,newsd,retval,port,readsd;
	ssize_t n;
	struct sockaddr_in sa;
	int server,data_in_stdin,data_in_socket;
	struct hostent *hp;
	fd_set rfds;
        struct timeval tv;


	if(argc<=1){

		printf("Help:How to use mychat\n");
		printf("./mychat -x y z\n");
		printf("x can be 's' for the user that establishes the communication bus or 'c' for the user that connects to an already established connection.\n");
		printf("If you set the -s parameter then 'y' and 'z' must be left blank.\n");
		printf("If u set the -c parameter then y must be the IP adress of the machine that established connection and z must be the port of the process that established connection.\n");
		exit(1);
	}
	else {
		if(strcmp(argv[1],"-s")==0) server=1;
		else if(strcmp(argv[1],"-c")==0) server=0;
		else 
			{
			printf("Type ./mychat for help\n");
			exit(1);
			}
	}

	buf=malloc(1000*sizeof(char));
	
	if(server==1){

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


		/* Listen for the other incoming connection */
		if (listen(sd, TCP_BACKLOG) < 0) {
			perror("listen");
			exit(1);
		}

		}
	else{
		if(argc!=4){
			 printf("Type ./mychat for help\n");
			 exit(1);
		}
		
		hostname = argv[2];
		port = atoi(argv[3]);

		/* Create TCP/IP socket, used as main chat channel */
		if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
			perror("socket");
			exit(1);
		}
		readsd=sd;
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
	}
		/*Code from this point and beyond is common for "server" and client"*/
		
		
		for(;;){
			if (server==1)
			{
				fprintf(stderr, "Waiting for an incoming connection...\n");
				/* Accept an incoming connection */
				len = sizeof(struct sockaddr_in);
				if ((newsd = accept(sd, (struct sockaddr *)&sa, &len)) < 0) {
					perror("accept");
					exit(1);
				}
				readsd=newsd;
				if (!inet_ntop(AF_INET, &sa.sin_addr, addrstr, sizeof(addrstr))) {
					perror("could not format IP address");
					exit(1);
				}
				fprintf(stderr, "Incoming connection from %s:%d\n",addrstr, ntohs(sa.sin_port));
			}
			for(;;){
				/*Getting ready for select*/
				FD_ZERO(&rfds);
	          		FD_SET(0, &rfds);
				FD_SET(readsd,&rfds);
				tv.tv_sec=1;
				tv.tv_usec=0;
	
				retval=select(readsd+1,&rfds,NULL,NULL,&tv);
	
				if(retval==-1){
					//Something is wrong with Select command
					perror("select");
					exit(1);
				}
				else if(retval>0){			
					//Select says that we have data
					if(FD_ISSET(readsd,&rfds))	data_in_socket=1;
					else data_in_socket=0;
				
					if(FD_ISSET(0,&rfds))	data_in_stdin=1;
					else data_in_stdin=0;
					//data_in_socket:Boolean variable,states if there is data in socket
					//data_in_stdin:Boolean variable,states if there is data from terminal
					if( (data_in_socket!=0) || (data_in_stdin!=0) ) {
																			 
						if(data_in_socket==1){
						n = read(readsd, buf, sizeof(buf));
						if (n <= 0) {
							if (n < 0)
								perror("read from remote peer failed");
							else
								fprintf(stderr, "Peer went away\n");
							break;
						}
						fprintf(stdout, "The other user said:%s\n", buf);
						}
						if( data_in_stdin==1){
							n = read(0,buf,sizeof(buf));
							if(n<0)	perror("read from terminal failed");
						
							buf[n-1]='\0';        //avoid overunning the buffer
							if (insist_write(readsd, buf, n) != n) {
								perror("write to remote peer failed");
								break;
							}
						}		
					}
				}
			}
		/*We decided that the "server" user should hang waiting for someone to connect.
		 *If you are not server then you should not hang in this bigger infinite loop
		*/
		if (server==0) break;
		 
		}
		return 1;
}
