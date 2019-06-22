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

#define FILE_TO_OPEN "/dev/LedMatrix"
#define COMMAND 1

#define MAX_BUF 64
#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define EDGE_PATH   "/sys/class/gpio/gpio14/edge"
#define ECHO_PATH "/sys/class/gpio/gpio14/value"
#define TRIGGER_PATH "/sys/class/gpio/gpio13/value"


// Pattern buffer to send the patterns to kernel space through 1octl call
uint16_t patternToSend[10][8] = {{0x0144,0x0238,0x0310,0x0438,0x0538,0x0610,0x0700,0x0800},
                                 {0x0100,0x0228,0x0328,0x0410,0x0538,0x0638,0x0710,0x0800},
                                 {0x0140,0x02c8,0x03f0,0x0470,0x05f0,0x06c8,0x0740,0x0800},
                                 {0x0140,0x0262,0x033c,0x043c,0x053c,0x0662,0x0740,0x0800},
                                 {0x0180,0x0260,0x03f0,0x0470,0x05e0,0x0640,0x07e0,0x0860},
                                 {0x0160,0x02f0,0x03f0,0x0460,0x05c0,0x06e0,0x0760,0x0800},
                                 {0x011c,0x020a,0x030e,0x040c,0x0508,0x060c,0x0708,0x0800},
                                 {0x0108,0x021c,0x0308,0x0428,0x0538,0x0618,0x070c,0x0808},
                                 {0x01b0,0x0288,0x034a,0x043d,0x052a,0x06c8,0x0784,0x0807},
                                 {0x0107,0x0208,0x03ca,0x043d,0x052a,0x06c8,0x0798,0x0820}};

// sequence buffer to carry the animation sequence for each pattern
int sequence[10];
// Animation sequence for Fish
int sequence1[10] = {0,200,1,200,0,200,1,200,0,0};
// Animation sequence for Frog
int sequence2[10] = {2,200,3,200,2,200,3,200,0,0};
// Animation sequence for Tortoise
int sequence3[10] = {4,200,5,200,4,200,5,200,0,0};
// Animation sequence for Bird
int sequence4[10] = {6,200,7,200,6,200,7,200,0,0};
// Animation sequence for Man Dancing
int sequence5[10] = {8,200,9,200,8,200,9,200,0,0};

unsigned int distance;
// Polling variable
struct pollfd pollVar={0};

/****************************************************************
 * gpio_export to export the given gpio pin into sysfs
 ****************************************************************/
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


/****************************************************************
 * gpio_set_dir - setting direction to the Direction pin
 ****************************************************************/
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

/****************************************************************
 * gpio_set_value - setting value to the pin
 ****************************************************************/
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

/************************************************************************

Function to read Time Stamp Counter for Galileo Gen 2 Architecture x86

**************************************************************************/
unsigned long long int rdtsc(void)
{
   unsigned a, d;

   __asm__ volatile("rdtsc" : "=a" (a), "=d" (d));

   return ((unsigned long long)a) | (((unsigned long long)d) << 32);;
}

/************************************************************************

Function to do the necessary GPIO programming for the Ultrasonic Sensor

pin settings | shield Pin 2 as Trigger | Shield pin 3 as Echo

**************************************************************************/

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

/************************************************************************

Thread body for the Polling functionality to measure the distance between
the object and the sensor. Runs for every 300ms

**************************************************************************/

void *pollforSignal(){
    int edge, trigger, echo, ret;
    long long int firstInstance, time_diff;
    char buff;

    //Opening the file descripters for each Edge, Echo and Trigger paths
    printf((edge=open(EDGE_PATH,O_RDWR)) < 0 ? "Error in opening Edgepath":"");
    printf((echo=open(ECHO_PATH,O_RDWR)) < 0 ? "Error in opening Echo Path":"");
    printf((trigger=open(TRIGGER_PATH,O_RDWR)) < 0 ? "Error in opening Trigger Path":"");

    // Adding the events to detect
    pollVar.events = POLLPRI | POLLERR;
    // Assigning the Echo Path to the file descripter of the Poll Structure
    pollVar.fd = echo;

    while(1){
        //Writing Risisg edge to the echo pin
        printf((lseek(echo,0,SEEK_SET)) < 0 ? "Error in doing lseek to echo":"");
        printf((write(edge,"rising",sizeof("rising"))) < 0 ? "Error in writing Rising edge to Edge":"");

        // Creating a trigger pulse
        printf((write(trigger,"0",sizeof("0"))) < 0 ? "Error in writing to trigger pin":"");
        usleep(10);
        printf((write(trigger,"1",sizeof("1"))) < 0 ? "Error in writing to trigger pin":"");
        usleep(60);             // Timing for keeping the trigger pulse in ON State
        printf((write(trigger,"0",sizeof("0"))) < 0 ? "Error in writing to trigger pin":"");

        // Polling for Rising Edge
        ret = poll(&pollVar,1,1000);

        if(ret < 0 ){
            printf("Error in polling\n");
        } else if (ret == 0){
            printf("Polling Timeout");
        } else {
            // If only Rising Edge is detected
            if(pollVar.revents & POLLPRI){
                // Capturing the time at the instant when the rising edge is detected
                firstInstance = rdtsc();
                read(echo,&buff,1);
            }
            // Writing falling to the edge path of the IO pin 3
             printf((lseek(echo,0,SEEK_SET)) < 0 ? "Error in doing lseek to echo":"");
             printf((write(edge,"falling",sizeof("falling"))) < 0 ? "Error in writing Rising edge to Edge":"");

            // Now polling for the falling Edge
             ret = poll(&pollVar,1,1000);
            if(ret < 0 ){
                printf("Error in polling\n");
            } else if (ret == 0){
                printf("Polling Timeout");
            } else {
                // If only falling edge is detected
                if(pollVar.revents & POLLPRI){
                    // Calculating the difference between the time at which the Rising and Falling Edge is detected.
                    time_diff = rdtsc() - firstInstance;
                    read(echo,&buff,1);

                    // Calculating the distance based on combining the formula for HCSR04 Ultra sonic sensor
                    // and the clocking frequency of the Quark processor
                    distance = (time_diff * 340) / 8000000ul;

                    // Printing the distance for every 300ms
                    printf("The Distance - %d\n\n",distance);
                    usleep(300000);
                }
            }

        }


    }


}


/************************************************************************

Thread body for sending the patterns through the SPI device driver based on the distance measured

**************************************************************************/

void *doSPI(){
    int spi_fd;
    // Opening the device driver file
    spi_fd = open(FILE_TO_OPEN, O_RDWR);

    if(spi_fd < 0){
        printf("\nCouldn't open the file\n");
    }

    // Sending the predefined pattern buffer to the kernel space
    if(ioctl(spi_fd, COMMAND,(uint16_t**)patternToSend) < 0){
        printf("\n IOCTL call Failed.\n");
    }

    while(1){
        // For Distances more than 100cm, Swimming Fish is displayed
        if( distance > 100 ){
            printf( (write(spi_fd,(int*)sequence1,sizeof(sequence1)) < 0) ? "\nWrite Function failed.\n":"");
        }
        // For distances between 70cm and 100cm, Jumping Frog is displayed
        else if(distance > 70 && distance <= 100){
            printf( (write(spi_fd,(int*)sequence2,sizeof(sequence2)) < 0) ? "\nWrite Function failed.\n":"");
        }

        // For Distances between 50cm and 70cm, moving Tortoise is displayed
        else if(distance > 50 && distance <= 70){
            printf( (write(spi_fd,(int*)sequence3,sizeof(sequence3)) < 0) ? "\nWrite Function failed.\n":"");
        }

        // For distances between 50cm and 25cm, flying bird is displayed
        else if(distance > 25 && distance <= 50){
            printf( (write(spi_fd,(int*)sequence4,sizeof(sequence4)) < 0) ? "\nWrite Function failed.\n":"");
        }

        // For distances below 25cm, Dancing human is displayed
        else{
            printf( (write(spi_fd,(int*)sequence5,sizeof(sequence5)) < 0) ? "\nWrite Function failed.\n":"");
        }
    }
    // To close the file descripter after the SPI operations are carried out which will call the release function in the kernel
    close(spi_fd);
}

int main(){

    pthread_t sonic_thread, led_thread;
    // Calling the function to prepare the GPIO for the ultrasonic sensor
    prepareHCSensor();

    // Creating the independent threads for measuring the distance as well as sending the animation patterns
    pthread_create(&sonic_thread,NULL,pollforSignal, NULL);
    pthread_create(&led_thread,NULL,doSPI,NULL);
    // Joining the sensor thread to prevent the main calling thread from ending abruptly
    pthread_join(sonic_thread,NULL);
    return 0;
}
