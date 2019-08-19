 /*
 * socket−server .c 
 * Simple TCP/IP communication using sockets 
 * 
 * Vangelis Koukis <vkoukis@cslab . ece . ntua .gr> 
 */ 
 
 #include <stdio .h> 
 #include <errno .h> 
 #include <ctype .h> 
 #include <string .h> 
 #include <stdlib .h> 
 #include <signal .h> 
 #include <unistd .h> 
 #include <netdb .h> 
 #include <stdbool .h> 
 #include <sys/time .h> 
 #include <sys/types .h> 
 #include <sys/ socket .h> 
 #include <arpa/ inet .h> 
 #include <netinet /in .h> 
 #include <crypto/cryptodev .h> 
 #include <sys/ ioctl .h> 
 #include <sys/ stat .h> 
 #include <fcntl .h> 
 #include ”socket−common.h”  

 /* Insist until all of the data has been written */ 

 ssize_t insist_write ( int fd , const void *buf , size_t cnt ) { 
	ssize_t ret ; 
	size_t orig_cnt = cnt ; 
	
	while ( cnt > 0) { 
		ret = write (fd , buf , cnt ) ; 
		if ( ret < 0) 
			return ret ; 
		buf += ret ; 
		cnt −= ret ; 
	} 
	
	return orig_cnt ;
	
} 

int main(void ) { 
	char buf[DATA_SIZE]; 
	char addrstr [INET_ADDRSTRLEN]; 
	int sd , newsd , maxsd , cfd , i ; 
	int server_writes=0, server_reads =0; 
	ssize_t n; 
	socklen_t len ; 
	fd_set read_fd_set ; 
	struct sockaddr_in sa ; 
	struct timeval tmv; 
	struct session_op sess ; 
	struct crypt_op cryp ; 
	
	struct { 
		unsigned char in[DATA_SIZE] , 
		encrypted[DATA_SIZE] , 
		decrypted[DATA_SIZE] , 
		iv[BLOCK_SIZE] , 
		key[KEY_SIZE]; 
	} data ; 
	
	/* Make sure a broken connection doesn ’t kill us */
	signal (SIGPIPE , SIG_IGN) ; 

	/* Create TCP/IP socket , used as main chat channel */
	if (( sd = socket (PF_INET , SOCK_STREAM, 0) ) < 0) { 
		perror (”socket”) ; 
		exit (1) ; 
	} 
	fprintf ( stderr , ”Created ␣TCP␣ socket \n”) ; 

	/* Bind to a well−known port */ 
	memset(&sa , 0, sizeof (sa) ) ; 
	sa . sin_family = AF_INET; 
	sa . sin_port = htons(TCP_PORT) ; 
	sa . sin_addr . s_addr = htonl (INADDR_ANY) ; 
	if (bind(sd , ( struct sockaddr *)&sa , sizeof (sa) ) < 0) { 
		perror (”bind”) ; 
		exit (1) ; 
	} 
	fprintf ( stderr , ”Bound␣TCP␣ socket ␣ to ␣ port ␣%d\n” , TCP_PORT) ; 

	/* Listen for incoming connections */
	if ( listen (sd , TCP_BACKLOG) < 0) { 
		perror (” listen ”) ; 
		exit (1) ;
	} 
	
	cfd = open(”/dev/crypto” , O_RDWR) ; 
	if ( cfd < 0) { 
		perror (”open(/ dev/crypto )”) ; 
		return 1; 
	} 
	/*Key must be the same for client and server . We agree on it before starting communicate*/ 
	/*Same for initialization vector ( iv ) */ 
	for ( i =0; i<KEY_SIZE; i++) 
		data .key[ i]= ’x ’ ; 
		
	for ( i =0; i<BLOCK_SIZE; i++) 
		data . iv[ i]= ’y’ ; 

	/* Loop forever , accept () ing connections */
	for (;;) {
		fprintf ( stderr , ”Waiting ␣ for ␣an␣ incoming ␣ connection . . . \ n”) ; 
			
		/* Accept an incoming connection */ 
		len = sizeof ( struct sockaddr_in ) ;
		if ((newsd = accept (sd , ( struct sockaddr *)&sa , &len ) ) < 0) { 
			perror (”accept”) ; 
			exit (1) ; 
		} 

		if (! inet_ntop (AF_INET, &sa . sin_addr , addrstr , sizeof ( addrstr ) ) ) { 
			perror (”could ␣ not ␣ format ␣ IP ␣ address”) ; 
			exit (1) ; 
		} 
		fprintf ( stderr , ”Incoming ␣ connection ␣from␣%s:%d\n” , 
			addrstr , ntohs(sa . sin_port ) ) ; 
		memset(&sess , 0, sizeof ( sess ) ) ; 
		memset(&cryp , 0, sizeof (cryp) ) ; 
		/* 
		* Get crypto session for AES128 
		*/
		sess . cipher = CRYPTO_AES_CBC; 
		sess . keylen = KEY_SIZE; 
		sess .key = data .key; 
			
		if ( ioctl (cfd , CIOCGSESSION, &sess ) ) { 
			perror (” ioctl (CIOCGSESSION)”) ; 
			return 1;
		} 
			
		for (;;) { 
			tmv. tv_sec =1; 
			tmv. tv_usec=0; 
			FD_ZERO(&read_fd_set ) ; 
			FD_SET(0 , &read_fd_set ) ; 
			FD_SET(newsd,&read_fd_set ) ;
			maxsd=newsd; 

			if ( select (maxsd+1, &read_fd_set ,NULL, NULL, &tmv) <0){ 
				perror (” select ”) ; 
				exit (1) ; 
			} 
				
			/*Check if someone wrote something . It ’s either us or the other client */ 
			if (FD_ISSET(0 , &read_fd_set ) ) 
				server_writes =1; 
			else 
				server_writes =0; 

			if (FD_ISSET(newsd , &read_fd_set ) ) 	
				server_reads =1; 
			else 
				server_reads =0; 
			/* In case we wrote something in stdin we need to send it */

			if ( server_writes ) { 
				n=read (0 , buf , sizeof (buf) ) ; 
				if (n < 0) { 
					perror (”Reading ␣from␣command␣ line ”) ; 
					break ; 
				} 
				if (n>0) 
					buf[n−1]=’\0 ’ ; 
				else 
					buf[0]= ’\0 ’ ; 

				for ( i =0;i<DATA_SIZE; i++) 
					data . in[ i ] = buf[ i ]; 
					
				/*
				* Encrypt data . in to data . encrypted
				*/
				cryp . ses = sess . ses ; 
				cryp . len = sizeof ( data . in ) ; 
				cryp . src = data . in ;
				cryp . dst = data . encrypted ;
				cryp . iv = data . iv ; 
				cryp .op = COP_ENCRYPT; 

				if ( ioctl (cfd , CIOCCRYPT, &cryp) ) { 
					perror (” ioctl (CIOCCRYPT)”) ; 
					return 1; 
				} 
					
				for ( i =0;i<DATA_SIZE; i++) 
					buf[ i]=data . encrypted[ i ]; 

				if ( insist_write (newsd , data . encrypted , DATA_SIZE) != DATA_SIZE) { 
					perror (”write ␣ to ␣ remote ␣ peer ␣ failed ”) ; 
					break ; 
				} 
			} 
					
					/* In case the other client wrote something we need to read it */
			if ( server_reads ) { 
				n=read(newsd , buf , DATA_SIZE) ; 

				if (n <= 0) { 
					if (n < 0) 
						perror (”read ␣from␣ remote ␣ peer ␣ failed ”) ; 
					else 
						fprintf ( stderr , ”Peer ␣went␣away\n”) ; 
					break ; 
				} 

				for ( i =0;i<DATA_SIZE; i++) 
					data . encrypted[ i]=buf[ i ]; 

				printf (”Encrypted ␣ data :\n”) ;
				for ( i =0;i<DATA_SIZE; i++) 
					printf (”%x” ,data . encrypted[ i ]) ;
				printf (”\n”) ;
							
				/* 
				* Decrypt data . encrypted to data . decrypted
				*/
				cryp . ses = sess . ses ; 
				cryp . len = sizeof ( data . encrypted ) ; 
				cryp . src = data . encrypted ; 
				cryp . dst = data . decrypted ; 
				cryp . iv = data . iv ;
				cryp .op = COP_DECRYPT; 

				if ( ioctl (cfd , CIOCCRYPT, &cryp) ) { 
					perror (” ioctl (CIOCCRYPT)”) ;
					return 1; 
				} 

				for ( i =0;i<DATA_SIZE; i++)
					buf[ i]=data . decrypted[ i ]; 
							
				fprintf ( stdout , ”(Decrypted ␣ data ) Client ␣ said:%s\n” , buf) ; 
			} 
		} 
					
		/* Make sure we don ’t leak open files */
		if ( close (newsd) < 0) 
			perror (”close”) ; 
					
		/* Finish crypto session */ 
		if ( ioctl (cfd , CIOCFSESSION , &sess . ses ) ) { 
			perror (” ioctl (CIOCFSESSION)”) ; 
			return 1; 
		} 
	} 
	if ( close ( cfd ) < 0) { 
		perror (”close ( fd )”) ; 
		return 1; 
	} 

	/* This will never happen */
	return 1;  
} 