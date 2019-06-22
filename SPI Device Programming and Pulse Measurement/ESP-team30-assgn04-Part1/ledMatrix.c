#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <time.h>
#include <poll.h>
#include <pthread.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#define SPIDEVICE "/dev/spidev1.0"      // opening the Spidev to access the spi operation from the user space

typedef struct
{
	uint8_t led[16];         // 16 bytes for 64 leds
} PATTERN;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0])) //array to store and send the patterns
#define MAX_BUF 64
#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define EDGE_PATH   "/sys/class/gpio/gpio14/edge"
#define ECHO_PATH "/sys/class/gpio/gpio14/value"
#define TRIGGER_PATH "/sys/class/gpio/gpio13/value"


int fd15, fd_spi;                   //     File descriptors
struct spi_ioc_transfer tr = {0};   //    The transfer is initialized with zeros
uint8_t transfer[2];
struct pollfd pollVar={0};
int patternFlag = 1, exiter=1;
unsigned int distance, oldDist, dist_difference;
unsigned long int delay = 500000;

PATTERN parting1 =
    {{
            0x01,0x96, // Pattern1 for animation to part away
            0x02,0x7e,
            0x03,0x90,
            0x04,0x00,
            0x05,0x00,
            0x06,0x90,
            0x07,0x7e,
            0x08,0x96,
    }};

PATTERN parting2 =
    {{
            0x01,0x26, // Pattern2 for the animation to part away
            0x02,0xfe,
            0x03,0x20,
            0x04,0x00,
            0x05,0x00,
            0x06,0x20,
            0x07,0xfe,
            0x08,0x26,
    }};

PATTERN joining1 =
    {{
            0x01,0xa0, // Pattern1 to join the animation
            0x02,0x7e,
            0x03,0xa6,
            0x04,0x00,
            0x05,0x00,
            0x06,0xa6,
            0x07,0x7e,
            0x08,0xa0,
    }};

PATTERN joining2 =
    {{
            0x01,0x00, // Pattern2 to join the animation
            0x02,0x10,
            0x03,0xfe,
            0x04,0x16,
            0x05,0x26,
            0x06,0xfe,
            0x07,0x20,
            0x08,0x00,
    }};

PATTERN heart1 =
    {{
            0x01,0x00, // Pattern1 to dispay animation of the heart
            0x02,0x18,
            0x03,0x3c,
            0x04,0x7c,
            0x05,0xf8,
            0x06,0x7c,
            0x07,0x3c,
            0x08,0x18,
    }};

PATTERN heart2 =
    {{
            0x01,0x00, // Pattern 2 to display animation of the heart
            0x02,0x00,
            0x03,0x18,
            0x04,0x38,
            0x05,0x70,
            0x06,0x38,
            0x07,0x18,
            0x08,0x00,
    }};

PATTERN zero =
    {{
            0x01,0x00, // Pattern bytes to turn off the led
            0x02,0x00,
            0x03,0x00,
            0x04,0x00,
            0x05,0x00,
            0x06,0x00,
            0x07,0x00,
            0x08,0x00,
    }};



//Exporting function to export the gpio pins
int gpio_export(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];

	fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
	if (fd < 0) {
		perror("gpio/export");
		return fd;
	}

	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);

	return 0;
}


//gpio_set_dir function for setting the direction of the Direction pin
int gpio_set_dir(unsigned int gpio, unsigned int out_flag)
{
	int fd;
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR  "/gpio%d/direction", gpio);
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/direction");
		return fd;
	}

	if (out_flag)
		write(fd, "out", 3);
	else
		write(fd, "in", 2);

	close(fd);
	return 0;
}

//set value function for the gpio pins
int gpio_set_value(unsigned int gpio, unsigned int value)
{
	int fd;
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);

	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/set-value");
		return fd;
	}

	if (value)
		write(fd, "1", 1);
	else
		write(fd, "0", 1);

	close(fd);
	return 0;
}

//rdtsc function to record the timestamps at required time
unsigned long long int rdtsc(void)
{
   unsigned a, d;

   __asm__ volatile("rdtsc" : "=a" (a), "=d" (d));

   return ((unsigned long long)a) | (((unsigned long long)d) << 32);;
}

/******************************************************************

Function to prepare the GPIO pins for the Ultrasonic Sensor

*******************************************************************/
void prepareSPIShieldPins(){

    //Setting up MOSI
    gpio_export(24);
    gpio_export(44);
    gpio_export(72);
    gpio_set_dir(24,1);
    gpio_set_dir(44,1);
    gpio_set_value(24,0);
    gpio_set_value(44,1);
    gpio_set_value(72,0);

    //Setting up SS
    gpio_export(15);
    gpio_export(42);
    gpio_set_dir(15,1);
    gpio_set_dir(42,1);
    gpio_set_value(15,0);
    gpio_set_value(42,0);

    //Setting up SCK
    gpio_export(30);
    gpio_export(46);
    gpio_set_dir(30,1);
    gpio_set_dir(46,1);
    gpio_set_value(30,0);
    gpio_set_value(46,1);

}

//  The Configuration data for the registers to activate the LED Driver for normal operation
uint8_t regConfigData[10] = {0x0C,0x01,0x09,0x00,0x0A,0x0F,0x0B,0x07,0x0F,0x00};


/********************************************************

Function to select/deselect the slave gate

*********************************************************/
int slaveSelectGate(char *key){
    if(write(fd15,key,1)<0){
        printf("\n\n Error Writing values to Slave Select Line");
        return -1;
    }
    return 0;
}


/********************************************************

Transferring the SPI data for LED Operation

*********************************************************/

void transferringSPIData(uint8_t arr[],unsigned int length){
    int err,i;
    for(i = 0 ; i < length;i+=2){
        transfer[0] = arr[i]; //to store the address
        transfer[1] = arr[i+1]; //to store the data
        slaveSelectGate("0");
        err = ioctl(fd_spi, SPI_IOC_MESSAGE(1),&tr);  //sending the data to the led display
        if(err < 0){                                 //Returns zero if an error is occured during the transfer of the pattern
            printf("\n\n Error in transferring the SPI message+ %d",errno);
        }
        usleep(1000);
        slaveSelectGate("1");
    }
}


/********************************************************

SPI Thread body for executing the SPI transfer opeartion

*********************************************************/

void *doSPI(){                                               //Thread to display the 8x8 led display
    fd15 = open("/sys/class/gpio/gpio15/value", O_WRONLY);
    if (fd15<0)
    {
        printf("\n gpio46 value open failed");
    }
    if((fd_spi = open(SPIDEVICE,O_WRONLY)) < 0){                    //opening the spi device
        printf("\n\nError Opening the SPI file in dev");
    }

    tr.tx_buf =(unsigned long) transfer;                    //setting the spi parameters
    tr.len = 2;
    tr.rx_buf = 0;
    tr.delay_usecs = 1;
    tr.cs_change = 1;
    tr.speed_hz = 10000000;
    tr.bits_per_word = 8;

    transferringSPIData(regConfigData,ARRAY_SIZE(regConfigData));  //transferring the data via spi
    while(exiter){
        if(distance > 0 && distance <= 20){                           //Displays heart when the distance is between 0 and 20
            transferringSPIData(heart1.led,ARRAY_SIZE(heart1.led));
            usleep(delay);
            transferringSPIData(heart2.led,ARRAY_SIZE(heart2.led));
            usleep(delay);                                            //delay is given between the display of patterns
        } else if (distance > 20 && distance <= 60){                   //Displays Joining of pair when the distance is between 20 and 60
            transferringSPIData(joining1.led,ARRAY_SIZE(joining1.led));
            usleep(delay);
            transferringSPIData(joining2.led,ARRAY_SIZE(joining2.led));
            usleep(delay);
        } else {                                                       //Displays partition of the pair when the distance is greater than 60
            transferringSPIData(parting1.led,ARRAY_SIZE(parting1.led));
            usleep(delay);
            transferringSPIData(parting2.led,ARRAY_SIZE(parting2.led));
            usleep(delay);
        }
    }

    transferringSPIData(zero.led,ARRAY_SIZE(zero.led));                 //turning off the led

    close(fd15);
    close(fd_spi);
    return NULL;
}


/********************************************************

Preparing the GPIO pins for the Ultrasonic Sensor
 IO 2 as Trigger
 IO 3 as Echo

*********************************************************/

void prepareHCSensor(){
    //Setting IO 2 as Trigger
    gpio_export(13);
    gpio_export(34);
    gpio_export(77);

    gpio_set_dir(13,1);
    gpio_set_dir(34,1);

    gpio_set_value(13,0);
    gpio_set_value(34,0);
    gpio_set_value(77,0);

    // Setting IO 3 as Echo
    gpio_export(14);
    gpio_export(16);
    gpio_export(76);
    gpio_export(64);

    gpio_set_dir(14,0);
    gpio_set_dir(16,0);

    gpio_set_value(16,1);
    gpio_set_value(76,0);
    gpio_set_value(64,0);

}


/********************************************************

Thread body for the polling functionality

*********************************************************/

void *pollforSignal(){                                   //Thread function for measuring the distance
    int edge, trigger, echo, ret, hold_diff, hold_next = 0;
    long long int firstInstance, time_diff;
    char buff;

    //Opening the file descripters for each values
    printf((edge=open(EDGE_PATH,O_RDWR)) < 0 ? "Error in opening Edgepath":"");
    printf((echo=open(ECHO_PATH,O_RDWR)) < 0 ? "Error in opening Echo Path":"");
    printf((trigger=open(TRIGGER_PATH,O_RDWR)) < 0 ? "Error in opening Trigger Path":"");

    pollVar.events = POLLPRI | POLLERR;
    pollVar.fd = echo;

    while(exiter){
        //Writing Risisg edge to the echo pin
        printf((lseek(echo,0,SEEK_SET)) < 0 ? "Error in doing lseek to echo":"");
        printf((write(edge,"rising",sizeof("rising"))) < 0 ? "Error in writing Rising edge to Edge":"");

        // Creating a trigger pulse
        printf((write(trigger,"0",sizeof("0"))) < 0 ? "Error in writing to trigger pin":"");
        usleep(10);
        printf((write(trigger,"1",sizeof("1"))) < 0 ? "Error in writing to trigger pin":"");
        usleep(30);
        printf((write(trigger,"0",sizeof("0"))) < 0 ? "Error in writing to trigger pin":"");

        ret = poll(&pollVar,1,1000);        //polling for the rising edge

        if(ret < 0 ){                      //returns an error if there is an error while polling
            printf("Error in polling\n");
        } else if (ret == 0){                 //returns polling timeout when polling is not possible
            printf("Polling Timeout");
        } else {
            if(pollVar.revents & POLLPRI){
                firstInstance = rdtsc();
                read(echo,&buff,1);
            }
             printf((lseek(echo,0,SEEK_SET)) < 0 ? "Error in doing lseek to echo":"");
             printf((write(edge,"falling",sizeof("falling"))) < 0 ? "Error in writing Rising edge to Edge":"");

             ret = poll(&pollVar,1,1000);     //polling for the falling edge
            if(ret < 0 ){
                printf("Error in polling\n");   //returns an error if there is an error while polling
            } else if (ret == 0){
                printf("Polling Timeout");         //returns polling timeout when polling is not possible
            } else {
                if(pollVar.revents & POLLPRI){
                    time_diff = rdtsc() - firstInstance;       //calculating the Uptime of the edge
                    read(echo,&buff,1);
                    distance = (time_diff * 340) / 8000000ul;       //calculating distance based on the Up time
                    dist_difference = abs(oldDist - distance);         //The change in difference is calculated
                    hold_diff = hold_next - (rdtsc()/10000000);
                    printf("Object Measured Distance - %d \n",distance);
                    if (dist_difference > 12){

                    //speed of the animation is controlled using distance difference and delay is made to set appropriatley

                        if(dist_difference > 100){
                            delay = 25000;
                        } else if (dist_difference <= 100 && dist_difference > 50){
                            delay = 50000;
                        } else if (dist_difference <= 50 && dist_difference > 25){
                            delay = 100000;
                        } else{
                            delay = 200000;
                        }
                        hold_next = (rdtsc()/10000000)+50 ;
                    } else {
                        if(hold_diff < 0){
                            delay = 500000;
                        }
                    }
                    usleep(50000);
                }
                oldDist = distance;      //previous Distance is stored before iterating
            }

        }


    }

    return NULL;
}

/**

    Signal Handler that receives the Interrupt Signal when the user decides to  stop the Execution of the program

    Press Ctrl + C to activate this Signal Handler

*/
void handleSignal(int sig){
    exiter = 0;
}

/********************************************************

THE MAIN PROGRAM

*********************************************************/

int main(){

    pthread_t sonic_thread, led_thread;           //creating threads for displaying led and measuring the distance
    prepareSPIShieldPins();
    prepareHCSensor();
    // Link the handler function with the SIGINT signal i.e. is Signal Interrupt
    signal(SIGINT, handleSignal);
    pthread_create(&sonic_thread,NULL,pollforSignal, NULL);
    pthread_create(&led_thread,NULL,doSPI,NULL);
    pthread_join(led_thread,NULL);       //joining the threads
    return 0;
}
