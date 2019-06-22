#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/delay.h>

#define EINVAL          22      /* Invalid argument */
#define DEVICE_NAME "RGBLed"


// Data Structure to hold the values for LED Device Driver

struct led_dev{
    struct cdev charDev;
    unsigned int gpio[3];
    ktime_t onTime;             // ON Time
    ktime_t offTime;            // OFF Time
    unsigned int switchBits;    // Switch Bits
    char name[20];              // Device Name
}*led_devp;


// Data Structure to hold the input values

struct input{
unsigned int dutyPercent;
unsigned int pins[3];
}inp;


#define SAMPLE _IOW(25,0,struct input*)


// Declaring Common Global variables required for this module
static dev_t dev_number;
struct class *deviceClass;
static struct device *ledDevice;
static struct hrtimer hr_timer;
unsigned long timer_interval_ns = 20e6;
unsigned long basic_unit = 2e5;
unsigned int counter = 0;


// The GPIO Pin Configuration for the Digital Shield pins from 0 to 13
static const int pinConfig[14][6] = {{11,32,0,0,0,0},
                                    {12,28,45,0,0,0},
                                    {13,34,77,0,0,0},
                                    {14,16,76,0,64,0},
                                    {6,36,0,0,0,0},
                                    {0,18,66,0,0,0},
                                    {1,20,68,0,0,0},
                                    {38,-1,0,0,0,0},
                                    {40,0,0,0,0,0},
                                    {4,22,70,0,0,0},
                                    {10,26,74,0,0,0},
                                    {5,24,44,0,72,0},
                                    {15,42,0,0,0,0},
                                    {7,30,46,0,0,0}};

/**

    The Open Call to open the Device Driver file

*/
static int led_driver_open(struct inode *ino, struct file *fil){
    printk("opening device driver\n\n");
    return 0;
}

/**
    The Callback function is to alter the Duty Cycle ON and OFF time periodically according to the
    input duty cycle percentage provided in the user space.

*/

enum hrtimer_restart timer_callback( struct hrtimer *timer_for_restart )
{
  	ktime_t currtime , interval;
  	currtime  = ktime_get();
  	if (counter%2 == 0){
        // Operations to be carried out for ON Time
        interval = led_devp->onTime;
        gpio_set_value(led_devp->gpio[0],(led_devp->switchBits & 0x4));
        gpio_set_value(led_devp->gpio[1],(led_devp->switchBits & 0x2));
        gpio_set_value(led_devp->gpio[2],(led_devp->switchBits & 0x1));
  	} else if (counter%2 == 1){
        // Operations to be carried out for OFF Time
        interval = led_devp->offTime;
        gpio_set_value(led_devp->gpio[0],0);
        gpio_set_value(led_devp->gpio[1],0);
        gpio_set_value(led_devp->gpio[2],0);
  	}
  	counter++;

  	if (counter == 50){
        counter=0;
        return HRTIMER_NORESTART;
  	} else {
        hrtimer_forward(timer_for_restart, currtime , interval);
        return HRTIMER_RESTART;
	}
}


/**

    IOCTL control call to write the basic CONFIG command to the device driver to select the necessary
    GPIO pins.

*/

static long led_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
    int index;
    printk("Name of the Module is %s", led_devp->name);
    copy_from_user(&inp,(struct input*)arg,sizeof(struct input));

    if(inp.dutyPercent > 99 || inp.dutyPercent < 0){
        printk("\nInvalid Arguments\n");
        return EINVAL;
    }
    // Calculating ON time once
    led_devp->onTime = ktime_set(0,((long)inp.dutyPercent) * basic_unit);
    // Calculating OFF time once
    led_devp->offTime = ktime_sub(ktime_set(0,timer_interval_ns),led_devp->onTime);

    for(index = 0;index<3;index++){

        if(inp.pins[index] < 0 || inp.pins[index] > 13){
            printk("\nInvalid Arguments\n");
            return EINVAL;
        }
        //Configuring GPIO
        //Allocating the linux pins
        gpio_request(pinConfig[inp.pins[index]][0],"sysfs");
        gpio_direction_output(pinConfig[inp.pins[index]][0], 0);

        //Configuring direction pins
        if(pinConfig[inp.pins[index]][1] >= 0){
            gpio_request(pinConfig[inp.pins[index]][1],"sysfs");
            gpio_direction_output(pinConfig[inp.pins[index]][1], 0);
            gpio_set_value(pinConfig[inp.pins[index]][1],0);
        }

        // Allocating GPIO Multiplexing Pin 1
        if(pinConfig[inp.pins[index]][2]>0){
            gpio_request(pinConfig[inp.pins[index]][2],"sysfs");
            gpio_set_value(pinConfig[inp.pins[index]][2],pinConfig[inp.pins[index]][3]);
        }

        // Allocating GPIO Multiplexing Pin 2
        if(pinConfig[inp.pins[index]][4]>0){
            gpio_request(pinConfig[inp.pins[index]][4],"sysfs");
            gpio_set_value(pinConfig[inp.pins[index]][4],pinConfig[inp.pins[index]][5]);
        }

        // Assigning the basic GPIO pins to the module Data Structure
        led_devp->gpio[index] = pinConfig[inp.pins[index]][0];

    }

    // Initiating HR Timer
    hrtimer_init( &hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
    // Defining the callback function
    hr_timer.function = &timer_callback;

    return 0;
}

/**

    This function call is to close the existing device driver and free the gpio pins selected for the previous operation

*/

static int led_driver_release(struct inode *ino, struct file *fil){

    int index, ret;

    // Canceling the Started HR timer
    ret = hrtimer_cancel(&hr_timer);

    if(ret == -1){
        printk("The timer is running active\n");
    } else if (ret == 1){
        printk("The timer is active\n");
    } else {
        printk("\nThe timer got closed successfully\n");
    }

    // Freeing the carried GPIO Pins
    for(index = 0;index<3;index++){
        gpio_free(pinConfig[inp.pins[index]][0]);
        if(pinConfig[inp.pins[index]][1] >= 0){
            gpio_free(pinConfig[inp.pins[index]][1]);
        }

        if(pinConfig[inp.pins[index]][2]>0){
            gpio_free(pinConfig[inp.pins[index]][2]);
        }

        if(pinConfig[inp.pins[index]][4]>0){
            gpio_free(pinConfig[inp.pins[index]][4]);
        }

    }

    printk("\n\n RGBLed is closed..\n");

    return 0;
}

/**

    This function call is to write the Switch bits to enable the respective LED lights to blink
    during each sequence of 0.5 seconds

*/

static ssize_t led_driver_write(struct file *file, const char *buf,
           size_t count, loff_t *ppos){

    copy_from_user(&led_devp->switchBits,(unsigned int*) buf, sizeof(buf));
    // Start the initiated HR Timer
    hrtimer_start( &hr_timer, ktime_set(0,0), HRTIMER_MODE_REL);
  //  msleep(20);
    return 0;
}

/**

    This struct file_operations defines the required file operations that has to happen
    in this particular module

*/

static struct file_operations fops = {
    .owner		    = THIS_MODULE,                  /* Owner */
    .open		    = led_driver_open,              /* Open method */
    .unlocked_ioctl = led_driver_ioctl,             /* Input Output Control method */
    .write		    = led_driver_write,             /* Write method */
    .release	    = led_driver_release,           /* Release method */
};


/**

    The Initiation function is reponsible for initiating the Device Driver while loading it to the Kernel
    using necessary device driver parameters

*/


static int __init led_driver_init(void){
    int ret;

    if (alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME) < 0) {
                printk(KERN_DEBUG "Can't register device\n"); return -1;
    }

    deviceClass = class_create(THIS_MODULE, DEVICE_NAME);

    led_devp = kmalloc(sizeof(struct led_dev),GFP_KERNEL);

    if (!led_devp) {
		printk("Bad Kmalloc\n"); return -ENOMEM;
	}

	sprintf(led_devp->name, DEVICE_NAME);
	cdev_init(&led_devp->charDev, &fops);
	led_devp->charDev.owner = THIS_MODULE;

	ret = cdev_add(&led_devp->charDev, (dev_number), 1);

	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}

	ledDevice = device_create(deviceClass, NULL, MKDEV(MAJOR(dev_number), 0), NULL, DEVICE_NAME);
	printk("Led Device Driver Initialized.\n");
    return 0;
}

/**

    This exit call is to uninstall the device driver which got installed using the necessary parameters whatever it is

*/

static void __exit led_driver_exit(void){

	/* Release the major number */
	unregister_chrdev_region((dev_number), 1);

	/* Destroy device */
	device_destroy (deviceClass, MKDEV(MAJOR(dev_number), 0));
	cdev_del(&led_devp->charDev);
	kfree(led_devp);

	/* Destroy driver_class */
	class_destroy(deviceClass);

	printk("Led Device Driver removed.\n");
}


// Declaring the Init and Exit Functions
module_init(led_driver_init);
module_exit(led_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Prasanth Sukhapalli");
MODULE_DESCRIPTION("This is a module for the LED Device Driver");
