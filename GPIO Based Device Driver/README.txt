ESP-Team30- Assignment 3. A GPIO-based LED Driver in Linux

Steps to run the program:

1) unzip the package to any working directory in your machine or Galileo Board.

2) Install and setup the board according to the instructions provided in the following link:

https://www.dropbox.com/sh/7t0tf61zkxmtxxw/AAB4HiFN5YmckFe6-l2J6kkTa/Galileo%20Gen%202%20SDK?dl=0&preview=Setting-Up_Galileo_2018.pdf&subfolder_nav_tracking=1

3) There is only a single Makefile in the package which is written to compile both the programs UserMain.c and LEDDriverModule.c that is suitable for the board

The execution environment for this assignment will be Intel Galileo Gen 2 board, and make file is written to cross compile the existing code for it.


Please fix the static ip of the board as 192.168.1.5 since the scp command used in the makefile utilises this IP to transfer the code


4) After running the makefile which copies both the user space and kernel object code onto the board, try the following

	"insmod LEDDriverModule.ko"

The above command is used to install the device driver module into the kernel of the board. Then type the following and press enter:

	"/MainUser"

Now provide the input arguments as given in the Assignment question in a single line in the console. 

	4 integers :

	1 - > Duty Cycle Percentage

	2, 3, 4 -> Shield pins where you would connect the Red, Green and Blue pins of the LED to the board.

Please use duty cycle percentage above 0 and below 100.
Please use the shield pins from 0 to 13.
Other Parameters would return an error message "Invalid Arguments and exits"

After starting the program with valid values, click any of the mouse button connected to the Board anytime to restart the LED Sequence as mentioned in the Assignment question.

In order to exit the program, press Ctrl + C anytime. SIGINT stops the program.

After stopping the program, use the following command to uninstall the kernel module LEDDriverModule.ko

	"rmmod LEDDriverModule.ko".


Arduino pin configuration varies for both the assignments. Please go to the below link for pin config details:

https://www.dropbox.com/sh/7t0tf61zkxmtxxw/AAD0sE84bTZTjEMybwjc1VW8a/Galileo%20Gen%202%20SDK/Quark_documents?dl=0&preview=Gen2_pins.xlsx&subfolder_nav_tracking=1


The above instructions should provide the output required for the assignment question.


******************************************************
TEAM MEMBERS
******************************************************


1. Prasanth Sukhapalli		-	1215358560
2. Kai Yuan Leong 		- 	1215666010


******************************************************
