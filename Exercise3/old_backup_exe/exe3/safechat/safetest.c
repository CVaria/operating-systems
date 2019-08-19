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

#include <crypto/cryptodev.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "socket-common.h"

#define BLOCK_SIZE	16
#define MAX_SENTENCE    256
#define KEY_SIZE	16  /* AES128 */
//key is agreed because it is not safe to send the key via Internet
//const char key[KEY_SIZE] = { 's', 'a', 'f', 'e', 'c' , 'h' , 'a' , 't' , 't' , 'i' , 'n' , 'g', '_' , 'D' , '&' , 'T' };


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

/* Insist until all of the data has been read */
ssize_t insist_read(int fd, void *buf, size_t cnt)
{
        ssize_t ret;
        size_t orig_cnt = cnt;

        while (cnt > 0) {
                ret = read(fd, buf, cnt);
                if (ret < 0)
                        return ret;
                buf += ret;
                cnt -= ret;
        }

        return orig_cnt;
}
//fill_urandom_buff defined but not used
/*static int fill_urandom_buf(unsigned char *buf, size_t cnt)
{
        int crypto_fd;
        int ret = -1;
        crypto_fd = open("/dev/urandom", O_RDONLY);
        if (crypto_fd < 0)
                return crypto_fd;
        ret = insist_read(crypto_fd, buf, cnt);
        close(crypto_fd);
        return ret;
}
*/

int main(int argc, char *argv[])
{
	char buf[MAX_SENTENCE],*hostname,addrstr[INET_ADDRSTRLEN];;
	socklen_t len;
	int sd,newsd,retval,port,readsd,cfd,i;
	ssize_t n;
	struct sockaddr_in sa;
	int server,data_in_stdin,data_in_socket;
	struct hostent *hp;
	fd_set rfds;
    	struct timeval tv;
	
	struct session_op sess;
	struct crypt_op cryp;
	struct {
		unsigned char 	in[MAX_SENTENCE],
				encrypted[MAX_SENTENCE],
				decrypted[MAX_SENTENCE],
				iv[16],
				key[KEY_SIZE] ;//= { 's', 'a', 'f', 'e', 'c' , 'h' , 'a' , 't' , 't' , 'i' , 'n' , 'g', '_' , 'D' , '&' , 'T' };
	} data;


	
	if(argc<=1){

		printf("Help:How to use mychat\n");
		printf("./mychat -x y z\n");
		printf("x can be 's' for the user that establishes the communication bus or 'c' for the user that connects to an already established connection.\n");
		printf("If you set the -s parameter then 'y' and 'z' must be left blank.\n");
		printf("If u set the -c parameter then y must be the IP adress of the machine that established connection and z must be the port that the other machine is waiting for connection.\n");
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

	
	cfd = open("/dev/crypto", O_RDWR);
	if (cfd < 0) {
		perror("open(/dev/crypto)");
		return 1;
	}
	
	for(i=0;i<KEY_SIZE;i++)		data.key[i]='i';
	
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
																		
						//Initialization of encryption structs
						memset(&sess, 0, sizeof(sess));
						memset(&cryp, 0, sizeof(cryp));
																		
						if(data_in_socket==1){
							n = read(readsd, buf, MAX_SENTENCE);
							if (n <= 0) {
								if (n < 0)
									perror("read from remote peer failed");
								else
									fprintf(stderr, "Peer went away\n");
								break;
							}
							
							for(i=0;i<MAX_SENTENCE;i++)  data.encrypted[i]=buf[i];
							//initialization vector random chosen
							//if (fill_urandom_buf(data.iv, 16) < 0) {
							//	perror("getting data from /dev/urandom\n");
							//	return 1;
							//}
							//No random vector chosen
							for(i=0;i<BLOCK_SIZE;i++)
							{
								data.iv[i]='a';
							}
						
							/*
							* Get crypto session for AES128
							*/
							sess.cipher = CRYPTO_AES_CBC;
							sess.keylen = KEY_SIZE;
							sess.key = data.key;
	
							if (ioctl(cfd, CIOCGSESSION, &sess)) {
								perror("ioctl(CIOCGSESSION)");
								return 1;
							}
						
						
							/*
							* Decrypt data.encrypted to data.decrypted
							*/
							cryp.ses = sess.ses;
							cryp.len = sizeof(data.encrypted);
							cryp.src = data.encrypted;
							cryp.dst = data.decrypted;
							cryp.iv = data.iv;
							cryp.op = COP_DECRYPT;
							if (ioctl(cfd, CIOCCRYPT, &cryp)) {
								perror("ioctl(CIOCCRYPT)");
								return 1;
							}
						
							//buf = data.decrypted;
							for(i=0;i<MAX_SENTENCE;i++){
								buf[i]=data.decrypted[i];
							}
						
						
							fprintf(stdout, "The other user said:%s\n", buf);
							
							
							/* Finish crypto session */
							if (ioctl(cfd, CIOCFSESSION, &sess.ses)) {
								perror("ioctl(CIOCFSESSION)");
								return 1;
							}
						}
						if( data_in_stdin==1){
							n = read(0,buf,sizeof(buf));
							if(n<0)	perror("read from terminal failed");
							
							
							//initialization vector random chosen
							//if (fill_urandom_buf(data.iv, 16) < 0) {
							//	perror("getting data from /dev/urandom\n");
							//	return 1;
							//}
							for(i=0;i<BLOCK_SIZE;i++)
							{
								data.iv[i]='a';
							}
							
						
							buf[n-1]='\0';        //avoid overunning the buffer
							for(i=0;i<MAX_SENTENCE;i++)		data.in[i] = buf[i];
						
							
							/*
							* Get crypto session for AES128
							*/
							sess.cipher = CRYPTO_AES_CBC;
							sess.keylen = KEY_SIZE;
							sess.key = data.key;
	
							if (ioctl(cfd, CIOCGSESSION, &sess)) {
								perror("ioctl(CIOCGSESSION)");
								return 1;
							}
							
							
							/*
							* Encrypt data.in to data.encrypted
							*/
							cryp.ses = sess.ses;
							cryp.len = sizeof(data.in);
							cryp.src = data.in;
							cryp.dst = data.encrypted;
							cryp.iv = data.iv;
							cryp.op = COP_ENCRYPT;

							if (ioctl(cfd, CIOCCRYPT, &cryp)) {
								perror("ioctl(CIOCCRYPT)");
								return 1;
							}
							
							for(i=0;i<MAX_SENTENCE;i++){
								buf[i]=data.encrypted[i];
							}						
	
							if (insist_write(readsd, data.encrypted, MAX_SENTENCE) != MAX_SENTENCE) {
								perror("write to remote peer failed");
								break;
							}
							
							/* Finish crypto session */
							if (ioctl(cfd, CIOCFSESSION, &sess.ses)) {
								perror("ioctl(CIOCFSESSION)");
								return 1;
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
		//Unreachable by code
		return 1;
}
