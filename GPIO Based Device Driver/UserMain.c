#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/ioctl.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#define MOUSEPATH "/dev/input/mice"
#define LED_Driver "/dev/RGBLed"


/**
    Data Structure the hold the input values

*/

struct input{
unsigned int dutyPercent;
unsigned int pins[3];
}inp;

#define SAMPLE _IOW(25,0,struct input*)

// Loop Flags getting initiated
unsigned int universal = 1, exiter = 1;
int fd;


/**
    The Mouse Thread Execution body that checks for any mouse clicks

*/

void *checkClick(void *ptr){

    int fid, hexBytes;
    unsigned char data[3];
    const char *mouseDevPath = MOUSEPATH;
    // Open MouseFile
    fid = open(mouseDevPath, O_RDWR);
    if(fid == -1)
    {
        printf("ERROR Opening the path - %s\n", mouseDevPath);
    }

    int left=0, right=0;

    while(1){
        //Reading content from the mouse file
        hexBytes = read(fid, data, sizeof(data));

        if(hexBytes > 0)
        {
            left = data[0] & 0x1;
            right = data[0] & 0x2;
            //Restart the sequence when user clicks left or right. set the flag accordingly
            if(left == 1 || right == 2){
                universal = 0;
                printf("\n Mouse Click Detected.\n");
            }

            if(exiter == 0){
                close(fd);
                printf("Closing the thread");
                break;
            }
        }
    }
    return NULL;
}

/**
    The Cycler function that executes a specific sequence of LED Display for a period of 0.5 seconds

*/
void cycler(int filp,unsigned int switchBits){
    if(universal > 0 && exiter > 0){
          write(filp,&switchBits,sizeof(switchBits));
          usleep(500000); // Sleeping for 0.5 seconds
    }
}

/**

    Signal Handler that receives the Interrupt Signal when the user decides to  stop the Execution of the program

    Press Ctrl + C to activate this Signal Handler

*/
void handleSignal(int sig){
    exiter = 0;
}

/*************************************************************************************************************

THE MAIN PROGRAM EXECUTION FOR THE USER SPACE

***************************************************************************************************************/

int main()
{
	int res;
	pthread_t mouseThread;

	// Getting inputs from the User
    scanf("%d %d %d %d",&inp.dutyPercent,&inp.pins[0],&inp.pins[1],&inp.pins[2]);


	/* open devices */
	fd = open(LED_Driver, O_RDWR);
	if (fd < 0 ){
		printf("Can not open device file.\n");
		return 0;
	} else {
        // Calling IOCTL function to write the BASIC Config command to select the shield pins for operation
        res = (int)ioctl(fd, SAMPLE,(struct input*) &inp);

        if(res!=0){
            printf("\n\n\nCouldn't do the IOCTL Operation");
            return res;
        }

        // Defining a periodic thread that checks for any mouse click events
        pthread_create( &mouseThread, NULL, checkClick, NULL);

        // Link the handler function with the SIGINT signal i.e. is Signal Interrupt
        signal(SIGINT, handleSignal);

        // Looping for periodically running the LED Glow Sequence
        while(exiter > 0){
            while(universal > 0 && exiter > 0){
                cycler(fd,000);             //  none, none, none
                cycler(fd,100);             //  Red, none, none
                cycler(fd,10);              //  none, Green, none
                cycler(fd,001);             //  none, none, Blue
                cycler(fd,110);             //  Red, Green, none
                cycler(fd,101);             //  Red, none, Blue
                cycler(fd,11);              //  none, Green, Blue
                cycler(fd,111);             //  Red, Green, Blue
            }
            universal = 1;
        }
	}

	// Calling the release function at the end to  destroy the footprints left by the IOCTL call
	if(close(fd) == 0)
        printf("Kernel File closed Successfully.\n\n");
	return 0;
}
