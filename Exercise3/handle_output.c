static void vq_handle_output ( VirtIODevice *vdev , VirtQueue *vq) {
	VirtQueueElement elem; 
	unsigned int * syscall_type ; 
	DEBUG_IN() ; 
	
	if (! virtqueue_pop (vq , &elem) ) { 
		DEBUG(”No␣ item ␣ to ␣pop␣from␣VQ␣ :( ”) ; 
		return ; 
	} 
	DEBUG(”I ␣have␣ got ␣an␣ item ␣from␣VQ␣ :) ”) ; 
	syscall_type = elem . out_sg [0]. iov_base ; 
	switch (* syscall_type ) { 
		case VIRTIO_CRYPTO_SYSCALL_TYPE_OPEN: 
			DEBUG(”VIRTIO_CRYPTO_SYSCALL_TYPE_OPEN”) ; 
			int *h_fd = elem . in_sg [0]. iov_base ; 
			*h_fd = open(”/dev/crypto” , O_RDWR) ; 
			if (* h_fd < 0) { 
				perror (”open(/ dev/crypto )”) ; 
				*h_fd=−2; 
			} 
			DEBUG(”DONE␣WITH␣ IT”) ; 
			break ; 
		case VIRTIO_CRYPTO_SYSCALL_TYPE_CLOSE: 
			DEBUG(”VIRTIO_CRYPTO_SYSCALL_TYPE_CLOSE”) ; 
			int *fd = elem . out_sg [1]. iov_base ; 
			if ( close (* fd ) <0){ 
				perror (”close”) ;
			} 
			break ; 
		case VIRTIO_CRYPTO_SYSCALL_TYPE_IOCTL: 
			DEBUG(”VIRTIO_CRYPTO_SYSCALL_TYPE_IOCTL”) ; 
			unsigned int *cmd = elem . out_sg [2]. iov_base ; 
			int *host_return_val ; 
			int *host_fd = elem . out_sg [1]. iov_base ;
			printf (”host_fd ␣=␣%d\n” , *host_fd ) ;
			switch (*cmd) { 
				case CIOCGSESSION: 
					DEBUG(”CIOCGSESSION”) ; 
					unsigned char *session_key=elem . out_sg [3]. iov_base ;
					struct session_op *sess=elem . in_sg [0]. iov_base ; 
					host_return_val = elem . in_sg [1]. iov_base ; 
					sess−>key = session_key ; 
					
					if ( ioctl (( int ) (* host_fd ) , CIOCGSESSION, sess ) ) {
						perror (” ioctl (CIOCGSESSION)”) ;
						*host_return_val=−1;
					} 
					printf (” after ␣ the ␣ ioctl \n”) ; 
					break ; 
				case CIOCFSESSION: 
					DEBUG(”CIOCFSESSION”) ; 
					uint32 *ses_id = elem . out_sg [3]. iov_base ;
					host_return_val = elem . in_sg [0]. iov_base ; 
					printf (”end␣ the ␣ crypto ␣ session \n”) ; 
					if ( ioctl (* host_fd , CIOCFSESSION , ses_id ) ) { 
						perror (” ioctl (CIOCFSESSION)”) ; 
						*host_return_val=−1; 
					} 
					break ; 
				case CIOCCRYPT: 
					DEBUG(”CIOCCRYPT”) ; 
					printf (”in ␣ ioctl ␣CIOCCRYPT\n”) ; 
					unsigned char *src = elem . out_sg [4]. iov_base ;
					unsigned char *iv = elem . out_sg [5]. iov_base ; 
					unsigned char *dst = elem . in_sg [0]. iov_base ; 
					struct crypt_op *cryp = elem . out_sg [3]. iov_base ; 
					host_return_val = elem . in_sg [1]. iov_base ; 
					cryp−>src = src ;
					cryp−>iv = iv ; 
					cryp−>dst = dst ; 
					
					if ( ioctl (* host_fd , CIOCCRYPT, cryp) ) {
						perror (” ioctl (CIOCCRYPT)”) ;
						*host_return_val=−1; 
					} 
					break ; 
				default : 
					DEBUG(”Unsupported ␣ ioctl ␣command”) ; 
					break ; 
				} 
			break ; 
		default : 
			DEBUG(”Unknown␣ syscall_type ”) ; 
	} 
		
	virtqueue_push (vq , &elem , 0) ; 
	virtio_notify (vdev , vq) ;
	DEBUG(”DONE”) ; 
}

