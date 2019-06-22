#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#define MAJOR_NUMBER 153
#define MINOR_NUMBER 0
#define DEVICE_NAME "LedMatrix"
#define CLASS_NAME "MatrixDisplay"
#define COMMAND 1

// User defined structure to store the device data
struct spidev_data {
	dev_t                   devt;
	struct spi_device       *spi;
}*spi_data;

static struct class *spi_class;
// SPI message structure to carry forward the message
static struct spi_message mess;
// Delay flag for guiding the SPI operation
int delayFlag = 0;

// Display buffer to hold patterns
uint16_t displayBuffer[10][8];
// Carrier to send the pattern data in a single SPI transfer
uint16_t transBuf[1];
// Configuration data for setting the 8x8 LED Driver for operation
uint16_t configData[5]={0x0C01,0x0900,0x0A0F,0x0B07,0x0F00};
// Sequence Buffer to hold the current data
int sequence[10];

// No Display data to turn off the LED Display
uint16_t noDisplay[8] = {0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700,0x0800};

// Structure to hold the necessary spi transfer parameters
struct spi_transfer tx = {

	.tx_buf = transBuf,         // Transmission Buffer
	.rx_buf = 0,                // setting receiving buffer as zero to have Half-Duplex Operation
	.len = 2,                   // No of Bytes
	.bits_per_word = 16,        // Bits per Word
	.speed_hz = 10000000,       // Speed of Transfer
	.delay_usecs = 1,           // Delay between Transfers
};


/*************************************************************************************************************
*
*
*   Funtion to send the SPI pattern to the LED matrix Driver
*
*
*
***************************************************************************************************************/

int send_spi_pattern(uint16_t passPattern[], int length){

    int i;
    for(i = 0;i < length; i++){
        transBuf[0]=passPattern[i];
        // Initiating the message variable
        spi_message_init(&mess);
        // Adding the message to the Tail
        spi_message_add_tail((void *)&tx, &mess);
        // Setting the Slave select pin as 0 to enable spi data transmission
        gpio_set_value(15,0);
        // Transferring the data through SPI synchronous operation
        if(spi_sync(spi_data->spi, &mess) < 0){
            printk("\nError in sending SPI message.\n");
            return -1;
        }

        // Setting the slave select pin as 1 to enable Display of transmitted data
        gpio_set_value(15,1);
	}
	return 0;
}

/*************************************************************************************************************
*
*
*   Funtion to open the LED matrix Driver and also to prepare the GPIO pins for SPI operation
        IO 11   as MOSI
        IO 12   as Slave Select
        IO 13   as Clock Pin for SPI
*
*
***************************************************************************************************************/

static int ledMatrixDriverOpen(struct inode *ino, struct file *fil){

    // Initiating the GPIO pins for SPI Operation

    // Configuring IO 13 as SPI_CLK
    gpio_export(30,true);
    gpio_export(46,true);
    gpio_direction_output(30,1);
    gpio_set_value(30,0);
    gpio_direction_output(46,1);
    gpio_set_value(46,1);

    // Configuring IO 11 as SPI_MOSI
    gpio_export(24,true);
    gpio_direction_output(24,1);
    gpio_set_value(24,0);

    gpio_export(44,true);
    gpio_direction_output(44,1);
    gpio_set_value(44,1);

    gpio_export(72,true);
    gpio_set_value(72,0);


    // Configuring IO 12 as Slave_Select
    gpio_export(15,true);
    gpio_direction_output(15,1);
    gpio_set_value(15,0);

    gpio_export(42,true);
    gpio_direction_output(42,1);
    gpio_set_value(42,0);

    // Sending the configuration data for Normal operation of the LED Display
    send_spi_pattern(configData, ARRAY_SIZE(configData));

    printk(KERN_INFO "\nInitiated all the necessary GPIO Pins for SPI Operation\n");

    return 0;

}


/*************************************************************************************************************
*
*
*   Closing function to release the GPIO pins taken during the operation
*
*
*
***************************************************************************************************************/

static int ledMatrixDriverClose(struct inode *ino, struct file *fil){
    gpio_unexport(30);
	gpio_free(30);
	gpio_unexport(46);
	gpio_free(46);
	gpio_unexport(24);
	gpio_free(24);
	gpio_unexport(44);
	gpio_free(44);
	gpio_unexport(72);
	gpio_free(72);
    gpio_unexport(15);
	gpio_free(15);
    gpio_unexport(42);
	gpio_free(42);

	printk(KERN_INFO "\nReleased all the necessary GPIO Pins for SPI Operation\n");
	return 0;
}


/*************************************************************************************************************
*
*
*   SPI probe function to create the defined SPI device under path /dev/
*
*
*
***************************************************************************************************************/

static int spi_probe_func(struct spi_device *spi)
{
	//struct spidev_data *spidev;
	int status = 0;
	struct device *dev;

	/* Allocate driver data */
	spi_data = kzalloc(sizeof(*spi_data), GFP_KERNEL);
	if(!spi_data)
	{
		return -ENOMEM;
	}

	/* Initialize the driver data */
	spi_data->spi = spi;
    // Assigning the Major and Minor Number
	spi_data->devt = MKDEV(MAJOR_NUMBER, MINOR_NUMBER);
    // Creating the character device
    dev = device_create(spi_class, &spi->dev, spi_data->devt, spi_data, DEVICE_NAME);

    if(dev == NULL)
    {
		printk("Device Creation Failed\n");
		kfree(spi_data);
		return -1;
	}
	printk("SPI LED Driver Probed.\n");
	return status;
}

/*************************************************************************************************************
*
*   Thread body for printing the patterns sent from the User Space
*
*
***************************************************************************************************************/

int spi_printing_thread(void *data){

    int i;
    // Setting the delay flag to avoid repeated calling of kernel threads before the termination of previous operations
    delayFlag = 1;
    for(i = 0; i < ARRAY_SIZE(sequence); i+=2){
        // Sending NO DISPLAY data at the end of the sequence
        if(sequence[i] == 0 && sequence[i+1] == 0){
            send_spi_pattern(noDisplay,ARRAY_SIZE(noDisplay));
            break;
        }
        // Sending the required SPI pattern
        send_spi_pattern(displayBuffer[sequence[i]],ARRAY_SIZE(displayBuffer[sequence[i]]));
        // Sleeping as per the delay given in the sequence
        msleep(sequence[i+1]);
    }
    delayFlag = 0;
    return 0;

}

/*************************************************************************************************************
*
*   Function to remove the spi device created
*
*
***************************************************************************************************************/

static int spi_remove(struct spi_device *spi){
	device_destroy(spi_class, spi_data->devt);
	kfree(spi_data);
	printk(" Driver Removed.\n");
	return 0;
}

/*************************************************************************************************************
*
*   IOCTL function to copy the pattern buffer from the User space to the pattern buffer in the kernel space
*
***************************************************************************************************************/

static long ledMatrixDriverIoctl(struct file *file, unsigned int cmd, unsigned long arg){
    copy_from_user(displayBuffer,(uint16_t **)arg, sizeof(displayBuffer));
    return 0;
}

/*************************************************************************************************************
*
*   Write function to write the sequence obtained from the user space
*
***************************************************************************************************************/

static ssize_t ledMatrixDriverWrite(struct file *file, const char *buf,
           size_t count, loff_t *ppos){
    struct task_struct *task;
    // If there exist already SPI thread operation, return busy
    if (delayFlag == 1){
        return EBUSY;
    }
    copy_from_user(sequence,(int*)buf,sizeof(sequence));
    // Running Kthread for SPI transfer
    task = kthread_run(&spi_printing_thread,(void *)sequence,"SPI Printing thread");
    msleep(20); // Sleeping for sometime to avoid kernel panic errors
    return 0;
}

/*************************************************************************************************************
*
*   SPI driver structure for SPI device
*
***************************************************************************************************************/
static struct spi_driver spi_ledMatrix_driver = {
         .driver = {
                 .name =         "spidev",
                 .owner =        THIS_MODULE,
         },
         .probe =        spi_probe_func,
         .remove =       spi_remove,

};

/*************************************************************************************************************
*
*  Structure to define the File operations for this Specific Driver
*
***************************************************************************************************************/

static const struct file_operations spi_file_ops = {

	.owner = THIS_MODULE,
	.open = ledMatrixDriverOpen,
	.release = ledMatrixDriverClose,
	.write	= ledMatrixDriverWrite,
	.unlocked_ioctl = ledMatrixDriverIoctl,

};

/*************************************************************************************************************
*
*   Device Driver Initiation function to register the Device Driver for the first time and also creating the
*   for the Driver
*
***************************************************************************************************************/

static int __init ledMatrixDriverInitiation(void){

    // Registering the SPI Character device with the already defined major number
    if(register_chrdev(MAJOR_NUMBER,spi_ledMatrix_driver.driver.name, &spi_file_ops) > 0){
        printk("Failed to register the SPI device\n");
        return -1;
    }

    // Creating a Device Driver Class
    spi_class = class_create(THIS_MODULE, CLASS_NAME);

    if (spi_class == NULL){
        printk("Class Creation failed.");
        unregister_chrdev(MAJOR_NUMBER, spi_ledMatrix_driver.driver.name);
        return -1;
    }

    // Registering the  SPI driver
    if(spi_register_driver(&spi_ledMatrix_driver) < 0){
        printk("SPI Driver Registration failed.");
        class_destroy(spi_class);
        unregister_chrdev(MAJOR_NUMBER, spi_ledMatrix_driver.driver.name);
    }
    printk("SPI LED Driver got initialised.\n");
    return 0;

}

/*************************************************************************************************************
*
*   Device Driver Exit function to be invoked when trying to remove the driver
*
***************************************************************************************************************/

static void __exit ledMatrixDriverUninstall(void){

    spi_unregister_driver(&spi_ledMatrix_driver);
    class_destroy(spi_class);
    unregister_chrdev(MAJOR_NUMBER, spi_ledMatrix_driver.driver.name);
    printk("SPI LED Driver has been removed\n");
}

module_init(ledMatrixDriverInitiation);
module_exit(ledMatrixDriverUninstall);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Prasanth Sukhapalli, Pradeep Chowdary Jampani");
MODULE_DESCRIPTION("This module is to control the SPI operations for a 8x8 LED matrix connected to the MAX7219");
