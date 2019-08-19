/*
 * lunix-chrdev.c
 *
 * Implementation of character devices
 * for Lunix:TNG
 *
 * < Your name here >
 *
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>

#include "lunix.h"
#include "lunix-chrdev.h"
#include "lunix-lookup.h"

/*
 * Global data
 */
struct cdev lunix_chrdev_cdev;

/*
 * Just a quick [unlocked] check to see if the cached
 * chrdev state needs to be updated from sensor measurements.
 */
static int lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	
	WARN_ON ( !(sensor = state->sensor));
	/* ? */

	if((sensor->msr_data[state->type]->last_update) != (state->buf_timestamp))
		return 1; 
	else
		/* The following return is bogus, just for the stub to compile */
		return 0; /* ? */
}

/*
 * Updates the cached state of a character device
 * based on sensor data. Must be called with the
 * character device state lock held.
 */
static int lunix_chrdev_state_update(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	uint16_t data;
	uint32_t time;
	long result=0, divi_t, mod_t;
	int flag;
	unsigned char sign;
	unsigned long zflags;
	int ret;
	 debug ("entering update\n");
	WARN_ON(!(sensor = state->sensor));

	/*
	 * Grab the raw data quickly, hold the
	 * spinlock for as little as possible.
	 */

// debug ("Waiting for lock type=%d\n", state->type);
	spin_lock_irqsave(&sensor->lock,zflags);
	/* Why use spinlocks? See LDD3, p. 119 */
		 //debug ("I'm on lock place ");
		flag=lunix_chrdev_state_needs_refresh(state);
		if(flag){
			debug("FOUND DATA\n");
			data = sensor->msr_data[state->type]->values[0];
			time = sensor->msr_data[state->type]->last_update;
		}
	spin_unlock_irqrestore(&sensor->lock, zflags);
	/*
	 * Any new data available?
	 */
	
	/*
	 * Now we can take our time to format them,
	 * holding only the private state semaphore
	 * (we already have it, from read function)
	 */
		
	/* flag =1 means i got new data else i didnt*/
	if(flag==1){
		//find raw data according to type of measurement
		switch(state->type){
		case 0:
			result = lookup_voltage[data];
			break;		
		case 1:
			result = lookup_temperature[data];
			break;
		case 2:
			result = lookup_light[data];
			break;
		case 3:
			flag = 2;
			break;
		}	

		 //debug ("result=%li flag=%d\n", result, flag);
		//define the sign of number
		debug("result before floating point %li\n", result);
		if((int)result>=0)
			sign=' ';
		else
			sign='-';

		divi_t = result / 1000;
		mod_t = result % 1000;
	 //debug ("sign =%c divi_t is %li and mod_t is %li\n",sign, divi_t, mod_t);
		
		snprintf(state->buf_data, LUNIX_CHRDEV_BUFSZ, "%c%d.%d\n", sign, divi_t , mod_t);
		state->buf_data[LUNIX_CHRDEV_BUFSZ-1]='\0';
		
		 //debug ("converting string %s", state->buf_data);
		/*find buf limit /size of measurement*/
		state->buf_lim = strnlen(state->buf_data, LUNIX_CHRDEV_BUFSZ);
	
		state->buf_timestamp = time;
		 //debug ("my new time update is %li\n", state->buf_timestamp);
		ret = 0;
		goto update_out;
	}
	else if (flag==0){
		ret = -EAGAIN;
		goto update_out;
	}
	else{
		ret = -ERESTARTSYS;
		goto update_out;
	}

update_out:	
	 //debug ("leaving update\n");
	return ret;
}
/*************************************
 * Implementation of file operations
 * for the Lunix character device
 *************************************/

static int lunix_chrdev_open (struct inode *inode, struct file *filp)
{
	/* Declarations */
	int ret;
	unsigned int minor;//my
	int tmp;
	 //debug ("entering\n");
	ret = -ENODEV;
	if ((ret = nonseekable_open(inode, filp)) < 0)
		goto out;

	/*
	 * Associate this open file with the relevant sensor based on
	 * the minor number of the device node [/dev/sensor<NO>-<TYPE>]
	 */
	 //debug ("Passed nonseekable\n");
	minor=iminor(inode);//my
		
	/* Allocate a new Lunix character device private state structure */
	struct lunix_chrdev_state_struct *point = kmalloc(sizeof(struct lunix_chrdev_state_struct), GFP_KERNEL); /*<my>*/
	if(point == 0)
		debug("Insufficient memory for state_struct in chrdev_open()\n");
	filp->private_data=point;
	//initialize allocated struct
	point->type=minor & 0x7;//minor=type of measurement (0=BATT, 1=TEMP, 2=LIGHT)
	 //debug ("fuck=%d\n", point->type);
	point->sensor = &lunix_sensors[minor/8];
	point->buf_lim = 0;
	point->buf_timestamp=0;	
	 tmp = minor/8;
 //debug ("sensor=%d\n", tmp);
	sema_init(&point->lock, 1);
	/*</my>*/
out:
	 //debug ("leaving, with ret = %d\n", ret);
	return ret;
}

static int lunix_chrdev_release(struct inode *inode, struct file *filp)
{
	kfree(filp->private_data);
	debug("Device successfully closed\n");
	return 0;
}

static long lunix_chrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* Why? */
	return -EINVAL;
}

static ssize_t lunix_chrdev_read(struct file *filp, char __user *usrbuf, size_t cnt, loff_t *f_pos)
{
	ssize_t ret;

	struct lunix_sensor_struct *sensor;
	struct lunix_chrdev_state_struct *state;

	state = filp->private_data;
	WARN_ON(!state);

	sensor = state->sensor;
	WARN_ON(!sensor);

	/* Lock? */
 debug ("Entered\n");
	if(down_interruptible(&state->lock)){
		return -ERESTARTSYS;
	}
	/*
	 * If the cached character device state needs to be
	 * updated by actual sensor data (i.e. we need to report
	 * on a "fresh" measurement, do so
	 */

	if (*f_pos == 0) {
		while (lunix_chrdev_state_update(state) == -EAGAIN) {
			/* ? */
			/* The process needs to sleep */
			/* See LDD3, page 153 for a hint */

			up(&state->lock); /* release the lock */

			if (filp->f_flags & O_NONBLOCK) //<- an theloume nonblocking politiki
				return -EAGAIN;
			 //debug ("Attempt to read: going to sleep\n");
			/*wait in wq queue until condition is true*/
			if (wait_event_interruptible(sensor->wq, (lunix_chrdev_state_needs_refresh(state) != 0))) 
				return -ERESTARTSYS; /* signal: tell the fs layer to handle it */

			/* otherwise loop, but first reacquire the lock */
			if (down_interruptible(&state->lock))
				return -ERESTARTSYS;
				
		}
	}

 //debug ("After update we found buf_lim=%d\n", state->buf_lim);
	/* End of file */
	
	if(state->buf_lim == 0){
		ret = 0;
		goto unlock_out;
	}
	
	/* Determine the number of cached bytes to copy to userspace */
	/* ? */
	if(cnt > (state->buf_lim - *f_pos)){
		cnt = (state->buf_lim )- (*f_pos);
	}	

	/* Auto-rewind on EOF mode? */
	/* ? */

 //debug ("Before copying to userspace cnt=%li and buf_lim=%li\n",cnt, state->buf_lim);
 //debug ("data buffer adress is %li\n", &state->buf_data);
	if(copy_to_user(usrbuf, state->buf_data + *f_pos, cnt)){
		ret = -EFAULT;
		 //debug ("something isnt right with copy_to_user\n");
		goto unlock_out;
	}
 //debug ("Reached here1");
	*f_pos += cnt;
	ret = cnt;



	if(*f_pos >= state->buf_lim){
		*f_pos=0;
	}
unlock_out:
	/* Unlock? */
	up(&state->lock);
 //debug ("ret = %li\n", ret);
 debug ("Leaving\n");
	return ret;
}

static int lunix_chrdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return -EINVAL;
}

static struct file_operations lunix_chrdev_fops = 
{
        .owner          = THIS_MODULE,
	.open           = lunix_chrdev_open,
	.release        = lunix_chrdev_release,
	.read           = lunix_chrdev_read,
	.unlocked_ioctl = lunix_chrdev_ioctl,
	.mmap           = lunix_chrdev_mmap
};

int lunix_chrdev_init(void)
{
	/*
	 * Register the character device with the kernel, asking for
	 * a range of minor numbers (number of sensors * 8 measurements / sensor)
	 * beginning with LINUX_CHRDEV_MAJOR:0
	 */
	int ret;
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3, range=16*3;//?? 2 sensors * 8 measurements
	
	 //debug ("initializing character device\n");
	cdev_init(&lunix_chrdev_cdev, &lunix_chrdev_fops);
	lunix_chrdev_cdev.owner = THIS_MODULE;
	lunix_chrdev_cdev.ops = &lunix_chrdev_fops; //is this needed?

	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	/* ? */
	
	/* register_chrdev_region? ela nte...*/
	ret = register_chrdev_region(dev_no, range, "Lunix"); 
	if (ret < 0) {
		debug("failed to register region, ret = %d\n", ret);
		goto out;
	}	
	/* ? */
	/*Add the device to the system*/
	ret = cdev_add(&lunix_chrdev_cdev, dev_no, lunix_minor_cnt);//my

	if (ret < 0) {
		debug("failed to add character device\n");
		goto out_with_chrdev_region;
	}
	debug("completed successfully\n");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
out:
	return ret;
}

void lunix_chrdev_destroy(void)
{
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;
		
	debug("entering\n");
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	cdev_del(&lunix_chrdev_cdev);
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
	debug("leaving\n");
}
