/*
 * crypto-chrdev.c
 *
 * Implementation of character devices
 * for virtio-crypto device 
 *
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 * Dimitris Siakavaras <jimsiak@cslab.ece.ntua.gr>
 * Stefanos Gerangelos <sgerag@cslab.ece.ntua.gr>
 *
 */
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>

#include "crypto.h"
#include "crypto-chrdev.h"
#include "debug.h"

#include "cryptodev.h"

#define KEY_SIZE 24
#define DATA_SIZE       16384
/*
 * Global data
 */
struct cdev crypto_chrdev_cdev;


/**
 * Given the minor number of the inode return the crypto device 
 * that owns that number.
 **/
static struct crypto_device *get_crypto_dev_by_minor(unsigned int minor)
{
	struct crypto_device *crdev;
	unsigned long flags;

	debug("Entering");

	spin_lock_irqsave(&crdrvdata.lock, flags);
	list_for_each_entry(crdev, &crdrvdata.devs, list) {
		if (crdev->minor == minor)
			goto out;
	}
	crdev = NULL;

out:
	spin_unlock_irqrestore(&crdrvdata.lock, flags);

	debug("Leaving");
	return crdev;
}

/*************************************
 * Implementation of file operations
 * for the Crypto character device
 *************************************/

static int crypto_chrdev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	int err;
	unsigned long flags;
	unsigned int len;
	struct crypto_open_file *crof;
	struct crypto_device *crdev;
	unsigned int syscall_type = VIRTIO_CRYPTO_SYSCALL_OPEN;
	int host_fd = -1;
	struct scatterlist syscall_type_sg, host_fd_sg, *sgs[2];

	debug("Entering");

	ret = -ENODEV;
	if ((ret = nonseekable_open(inode, filp)) < 0)
		goto fail;

	/* Associate this open file with the relevant crypto device. */
	crdev = get_crypto_dev_by_minor(iminor(inode));
	if (!crdev) {
		debug("Could not find crypto device with %u minor", 
		      iminor(inode));
		ret = -ENODEV;
		goto fail;
	}

	crof = kzalloc(sizeof(*crof), GFP_KERNEL);
	if (!crof) {
		ret = -ENOMEM;
		goto fail;
	}

	crof->crdev = crdev;
	crof->host_fd = -1;
	filp->private_data = crof;
	debug("kzalloc ok");
	/**
	 * We need two sg lists, one for syscall_type and one to get the 
	 * file descriptor from the host.
	 **/
	
	sg_init_one(&syscall_type_sg, &syscall_type, sizeof(syscall_type));
	sgs[0] = &syscall_type_sg;
	sg_init_one(&host_fd_sg, &host_fd, sizeof(host_fd));
	sgs[1] = &host_fd_sg;

	spin_lock_irqsave(&crdrvdata.lock, flags);
	err = virtqueue_add_sgs(crdev->vq, sgs, 1, 1, &syscall_type_sg, GFP_ATOMIC);
	//debug("after virtqueue_add");		

	if(err<0){
		debug("something wrong");
		ret = -ENODEV;
		goto fail;
	}

	virtqueue_kick(crdev->vq);
	/**
	 * Wait for the host to process our data.
	 **/
	while (virtqueue_get_buf(crdev->vq, &len) == NULL)
		/*do nothing*/;


	spin_unlock_irqrestore(&crdrvdata.lock, flags);
	/* If host failed to open() return -ENODEV. */
	if(host_fd <0){
		ret = -ENODEV;
		goto fail;
	}

	crof-> host_fd = host_fd;
	printk("host_fd = %d\n", crof->host_fd);
fail:
	debug("Leaving");
	return ret;
}

static int crypto_chrdev_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	int err;
	unsigned int len;
	unsigned long flags;
	struct crypto_open_file *crof = filp->private_data;
	struct crypto_device *crdev = crof->crdev;
	unsigned int syscall_type = VIRTIO_CRYPTO_SYSCALL_CLOSE;
	struct scatterlist syscall_type_sg, host_fd_sg, *sgs[2]; 

	debug("Entering");

	/**
	 * Send data to the host.
	 **/
	sg_init_one(&syscall_type_sg, &syscall_type, sizeof(syscall_type));
	sgs[0] = &syscall_type_sg;
	sg_init_one(&host_fd_sg, &(crof->host_fd), sizeof(crof->host_fd));
	sgs[1] = &host_fd_sg;

	spin_lock_irqsave(&crdrvdata.lock, flags);
	err = virtqueue_add_sgs(crdev->vq, sgs, 2, 0, 
	                        &syscall_type_sg, GFP_ATOMIC);		

	virtqueue_kick(crdev->vq);
	while (virtqueue_get_buf(crdev->vq, &len) == NULL)
		/*do nothing*/;
	spin_unlock_irqrestore(&crdrvdata.lock, flags);
	/**
	 * Wait for the host to process our data.
	 **/
	

	kfree(crof);
	debug("Leaving");
	return ret;

}

static long crypto_chrdev_ioctl(struct file *filp, unsigned int cmd, 
                                unsigned long arg)
{
	long ret = 0;
	int err;
	unsigned long flags;
	struct crypto_open_file *crof = filp->private_data;
	struct crypto_device *crdev = crof->crdev;
	struct virtqueue *vq = crdev->vq;
	struct scatterlist syscall_type_sg, output_msg_sg, input_msg_sg,*sgs[8];
	struct scatterlist host_fd_sg, ioctl_cmd_sg, session_key_sg, sess_sg, sess_id_sg, host_ret_sg, cryp_sg, src_sg, dst_sg, iv_sg;
	#define MSG_LEN 100

	unsigned char output_msg[MSG_LEN], input_msg[MSG_LEN];
	unsigned int num_out, num_in,
	             syscall_type = VIRTIO_CRYPTO_SYSCALL_IOCTL,
	             len;

	/*Common fields of CIOXXSESSION*/
	unsigned int ioctl_cmd=cmd;
	int host_return_val;
	/*Fields of CIOCGSESSION*/
	//struct session_op  *temp; 
	unsigned char session_key[KEY_SIZE];
	struct session_op sess;
	struct session_op * temp;	
	/*Fields of CIOCFSESSION*/
	u32 sess_id=crdev->minor;
	/*Fields of CIOCCRYPT*/
	struct crypt_op cryp;
	struct crypt_op * temp2;
	unsigned char *src=NULL;
	unsigned char iv[BLOCK_SIZE];
	unsigned char *dst=NULL;
	
	int i=0;
	debug("Entering");

	num_out = 0;
	num_in = 0;

	/**
	 *  These are common to all ioctl commands.
	 **/
	sg_init_one(&syscall_type_sg, &syscall_type, sizeof(syscall_type));
	sgs[num_out++] = &syscall_type_sg;
	sg_init_one(&host_fd_sg, &(crof->host_fd), sizeof((crof->host_fd)));
	sgs[num_out++] = &host_fd_sg;
	sg_init_one(&ioctl_cmd_sg, &ioctl_cmd, sizeof(ioctl_cmd));
	sgs[num_out++] = &ioctl_cmd_sg;
	/* ?? */

	/**
	 *  Add all the cmd specific sg lists.
	 **/
	switch (cmd) {
	case CIOCGSESSION:
		debug("CIOCGSESSION");
	
		temp = (struct session_op __user * ) arg;	
		printk("size of arg=%li\n", sizeof(*(temp)));
		printk("size of session op =%li\n", sizeof(struct session_op));	
		
		if(copy_from_user(&sess, temp, sizeof(*temp))){
			ret = -EFAULT;
	
		}
		printk("ses->keylen:%d||\n",sess.keylen);	
		if(copy_from_user(session_key, (__u8 __user *)(temp->key), sess.keylen*sizeof(unsigned char))){
			ret = -EFAULT;
		}
		printk("session_key:%s||\n",session_key);
		sg_init_one(&session_key_sg, session_key, sizeof(*session_key));
		sgs[num_out++] = &session_key_sg;
		sg_init_one(&sess_sg, &sess, sizeof(struct session_op));
		sgs[num_out + num_in++] = &sess_sg;
		break;

	case CIOCFSESSION:
		debug("CIOCFSESSION");
		sg_init_one(&sess_id_sg, &sess_id, sizeof(sess_id));
		sgs[num_out++] = &sess_id_sg;			

		break;

	case CIOCCRYPT:
		debug("CIOCCRYPT");
		temp2 = (struct crypt_op __user * ) arg;
		if(copy_from_user(&cryp, temp2, sizeof(*temp2))){
			ret = -EFAULT;
		}
		
		dst = kzalloc(sizeof(unsigned char)*cryp.len, GFP_KERNEL);
		if(!dst)
			ret = -ENOMEM;

		src = kzalloc(sizeof(unsigned char)*cryp.len, GFP_KERNEL);
		if(!src)
			ret = -ENOMEM;
		
		if(copy_from_user(src, (__u8 __user *)(temp2->src), cryp.len*sizeof(unsigned char))){
			ret = -EFAULT;
		}

		if(copy_from_user(iv, (__u8 __user *)(temp2->iv), BLOCK_SIZE*sizeof(unsigned char))){
			ret = -EFAULT;
		}

		if(copy_from_user(dst, (__u8 __user *)(temp2->dst), cryp.len*sizeof(unsigned char))){
			ret = -EFAULT;
		}

		sg_init_one(&cryp_sg, &cryp, sizeof(struct crypt_op));
		sgs[num_out++] = &cryp_sg;
		sg_init_one(&src_sg, src, sizeof(*src));
		sgs[num_out++] = &src_sg;
		sg_init_one(&iv_sg, iv, sizeof(*iv));
		sgs[num_out++] = &iv_sg;
		sg_init_one(&dst_sg, dst, sizeof(*dst));
		sgs[num_out + num_in++] = &dst_sg;
	

		break;

	default:
		debug("Unsupported ioctl command");

		break;
	}

	sg_init_one(&host_ret_sg, &host_return_val, sizeof(host_return_val));
	sgs[num_out + num_in++] = &host_ret_sg;

	
	spin_lock_irqsave(&crdev->crdev_lock, flags);
	err = virtqueue_add_sgs(crdev->vq, sgs, num_out, num_in, 
	                        &syscall_type_sg, GFP_ATOMIC);

	virtqueue_kick(crdev->vq);
	while (virtqueue_get_buf(crdev->vq, &len) == NULL)
		;
	
	spin_unlock_irqrestore(&crdev->crdev_lock, flags);

	switch(cmd){
		case CIOCCRYPT:
			temp2 = (struct crypt_op __user * ) arg;
			if(copy_to_user((__u8 __user *)(temp2->dst), dst, cryp.len*sizeof(unsigned char))){
				ret = -EFAULT;
			}
			break;
		case CIOCGSESSION:
			temp = (struct session_op __user * ) arg;	
			if(copy_to_user(temp, &sess, sizeof(*temp))){
				ret = -EFAULT;
			}
			break;
	}

	debug("Leaving");

	return ret;
}

static ssize_t crypto_chrdev_read(struct file *filp, char __user *usrbuf, 
                                  size_t cnt, loff_t *f_pos)
{
	debug("Entering");
	debug("Leaving");
	return -EINVAL;
}

static struct file_operations crypto_chrdev_fops = 
{
	.owner          = THIS_MODULE,
	.open           = crypto_chrdev_open,
	.release        = crypto_chrdev_release,
	.read           = crypto_chrdev_read,
	.unlocked_ioctl = crypto_chrdev_ioctl,
};

int crypto_chrdev_init(void)
{
	int ret;
	dev_t dev_no;
	unsigned int crypto_minor_cnt = CRYPTO_NR_DEVICES;
	
	debug("Initializing character device...");
	cdev_init(&crypto_chrdev_cdev, &crypto_chrdev_fops);
	crypto_chrdev_cdev.owner = THIS_MODULE;
	
	dev_no = MKDEV(CRYPTO_CHRDEV_MAJOR, 0);
	ret = register_chrdev_region(dev_no, crypto_minor_cnt, "crypto_devs");
	if (ret < 0) {
		debug("failed to register region, ret = %d", ret);
		goto out;
	}
	ret = cdev_add(&crypto_chrdev_cdev, dev_no, crypto_minor_cnt);
	if (ret < 0) {
		debug("failed to add character device");
		goto out_with_chrdev_region;
	}

	debug("Completed successfully");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, crypto_minor_cnt);
out:
	return ret;
}

void crypto_chrdev_destroy(void)
{
	dev_t dev_no;
	unsigned int crypto_minor_cnt = CRYPTO_NR_DEVICES;

	debug("entering");
	dev_no = MKDEV(CRYPTO_CHRDEV_MAJOR, 0);
	cdev_del(&crypto_chrdev_cdev);
	unregister_chrdev_region(dev_no, crypto_minor_cnt);
	debug("leaving");
}
