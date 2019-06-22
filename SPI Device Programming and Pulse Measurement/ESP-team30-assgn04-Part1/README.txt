ESP-Team30- Assignment 4 SPI Device Programming and Pulse Measurement (200 points)

: PART 1 ::

Steps to run the program:

GENERAL SETUP:

1) unzip the package to any working directory in your machine.

2) Install and setup the board according to the instructions provided in the following link:

https://www.dropbox.com/sh/7t0tf61zkxmtxxw/AAB4HiFN5YmckFe6-l2J6kkTa/Galileo%20Gen%202%20SDK?dl=0&preview=Setting-Up_Galileo_2018.pdf&subfolder_nav_tracking=1


ASSIGNMENT SETUP:

1) Follow the previously defined steps for connecting the board with the host machine.

2) Now connect the IO pins 2 and 3 to the  trigger and Echo pins of the sensor HC-SR04 and connect the pins IO 11, 12 and 13 to the MOSI, Chip-select and SCK-CLK pins of MAX7219 LED Driver.

COMPILATION:

1) Open the part-1 of the package, and type the "make" command. Before making the file, make sure the IP address of the board is "192.168.1.5" because our Makefile holds the command for file transfer.

2) The created object file is now transferred to the board.

EXECUTION:

1) In the root directory of the board, run the object file as follows:

	Type "./ledMatrix" and Enter.

2) Now the user space program would run printing the distance measured. Before running the program, please make sure that the space is cleared before the Ultrasonic sensor for upto 150 cm. 
Also it is recommended to place the sensor at the edge of the table or desk for proper distance measurement.

3) Let's now view the short Animation of a couple. Let's name them Romeo and Juliet for our convenience. Based on the object kept before the sensor, the following happens.

	For distances > 60cm, the couple part from each other and get separated.
	For distances between 20cm and 60cm, the couple will try to come together.
	For distances < 20cm, the love between the couple blossoms.

The speed at which the object is moved towards or away from the sensor decides the speed of the Animation. Now its upto the user to unite the couple or divide them forever.

4) At any time, to end the program, press Ctrl + C to terminate the program. The LED matrix blanks out.

It is required to maintain a minimum distance of 10cm(data tested based on our given sensor) between the object and the sensor for better operations.


Team members :
PRADEEP CHOWDARY JAMPANI (1215335693)
PRASANTH SUKHAPALLI (1215358560)
