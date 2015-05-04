/*
 * ELEC327 Final Project - Yuqiang Heng, Tingkai Liu, Yiqiu Wang
 *
 * Skateboard Speedometer with Light Painting Display
 *
 * This project uses hardware interrupt to count the number of wheel turn within a time window
 * controlled by WDT interrupt routine. After calculating the current speed of skateboard using
 * the number of wheel turns, we disable WDT and PORT ISR and display the pattern through
 * Timer Interrupt. This way we minimize energy usage and uses a seperate clock to easily control
 * the pattern of LED which is also adjusted to the speed since the faster the speed, the smaller the
 * timer interval between two columns of the pattern.
 *
 *
 */
#include <msp430.h>
/*
 * The LED_lib.h file contains the custom made library created to display numbers
 * It contains functions to Intialize LED display, Control color of individual APA102C SPI LED,
 * Display a column of LED, and End LED display
 */
#include "LED_lib.h"


// Pinout
#define HALL 	(BIT0) // p1.0 Hall sensor input
#define LED		(BIT2 + BIT4) // P1.2 and P1.4 are LED output

// Counters
int wheel_turn=0;
int wdt_counter=0;
int column = 0;//column counter for display

// Global Variable
#define DELAY    	(65000)//delay between two strings of led for each 1m/s is 0.015s

// Function prototypes
void set_led();		// this function sets up SPI LED, it uses SMCLK and 3-pin,  8-bit   SPI master mode
void set_Hall(void);// this function sets up P1.0 Hardware Interrupt for Hall Sensor analog input
void set_timer(int speed_set);// this function configures enalbes TIMER A1 interrupt in CCR0 CCIE mode,
							  //CCR0 value is adjusted for speed
// Variable
int speed = 0;//initial speed should be 0. just put here in case

/*
 * Main loop handles to most logic.
 * Here, speed is computed from wheel_turn counter after SPI LED finishes displaying pattern
 * It also disables PORT and WDT ISR when SPI LED is displaying pattern
 * and calls set_timer function to enable TIMERA1 ISR to display pattern when required.
 */
void main()
{
 	WDTCTL = WDTPW + WDTHOLD;        	// Stop WDT for configuration
 	BCSCTL1 = CALBC1_1MHZ; 				//calibrate DCO to 1Mhz
 	DCOCTL = CALDCO_1MHZ;

 	set_led(); 							//set up LED and Hall sensor
 	set_Hall();

 	WDTCTL = WDT_MDLY_32; 				//enable WDT to enter interrupt every 32ms
 	IE1 |= WDTIE;						//Enable WDT interrupt
 	while(1)
 	{
     	__bic_SR_register(CPUOFF + GIE);//Put system in LPM0 to wait till speed reading is acquired
     	P1IE &= ~HALL;					//disable PORT interrupt
     	IE1 &= ~WDTIE; 					//disable WDT interrupt
     	speed = wheel_turn>>3; 			//speed in m/s is roughly 1/8*number of wheel turns
       	wheel_turn=0; 					//reset wheel_turn counter
       	set_timer(speed);				//configure TIMER A1 interrupt to display the speed value
 	}
}

/*
 * This function configures and enable TIMER interrupt given speed measurement
 * The faster the board, the shorter the delay between two columns
 * Interrupt uses DCO as clock and operates in UP mode where it counts all the way up to the delay number
 */
void set_timer(int speed_set){
	TA1CCTL0 = CCIE;            	//Puts the timer control on CCR0
	TA1CCR0 = (int)(DELAY/speed_set);        	//200 cycles until first interrupt
	TA1CTL = TASSEL_2 + MC_1;	//Set Timer 0 Using ACLK to use DCO and set UP mode
}

/*
 *  PORT1 ISR handles the wheel_turn counter and records number of wheel_turns
 * The PORT1 ISR is entered when the magnet gets close to the hall sensor
 * The output to Hall sensor is active low, meaning the analog output is low only when the magnet is close to the sensor.
 * This ISR is only enabled when reading of speed is required
 * Each time the this isr is entered, wheel_turn is incremented by 1
 */
#pragma vector=PORT1_VECTOR
__interrupt void HALL_interrupt(void){
	wheel_turn++; 			//increment wheel turn counter
	P1IFG &= ~HALL; 		//clear P1 flag to exit ISR
}


/*
 * WDT timer handles the amount of time required to record wheel_turn and calculate speed
 * It is used to record time taken for a given number of wheel turns
 * this will help giving the speed that the skateboard is travelling as speed = wheel_turn/time
 */
#pragma vector = WDT_VECTOR
__interrupt void watchdog_timer(void){
	wdt_counter++;

	if(wdt_counter==30){	//every 30 interrupts(1080ms), calculate average speed and reset wheel_turn
    	wdt_counter = 0;	//reset wdt counter
   	__bic_SR_register_on_exit(CPUOFF); //exit LPM to configure timer and display pattern
	}
}

/*
 * TIMER A1 ISR handles the display of patterns
 * It calls a function corresponding to the speed to display and display it column by column
 * After all columns of the pattern has been displayed, it reenables WDT and PORT interrupt to get a new speed
 * Each number is displayed as 8 columns of LEDs
 */
#pragma vector = TIMER1_A0_VECTOR
__interrupt void timer_A1 (void){
	int speed_timer = speed; // Uses a different variable to ensure no change to speed variable is made accidentally

	if (column<=6){			//check if we have displayed all columns
 	ini_led();				//if not, we display the pattern by first initializing the LED
 	switch (speed_timer){	//We then display the pattern according to the speed
   	case 0:					//each case here calls a programmed function that displays the pattern
   	  zero(column);
   	  break;
   	case 1:
     	one(column);
     	break;
   	case 2:
     	two(column);
     	break;
   	case 3:
     	three(column);
     	break;
   	case 4:
     	four(column);
     	break;
   	case 5:
     	five(column);
     	break;
   	case 6:
     	six(column);
     	break;
   	case 7:
     	seven(column);
     	break;
   	case 8:
     	eight(column);
     	break;
   	case 9:
     	nine(column);
     	break;
 	}
 	end_led();
 	column++;				// we increment counter to display the corresponding column
	}
	else{					//if we have displayed all columns
 	column = 0;				//We will first clear column counter

 	TA1CCTL0 &= ~CCIE; 		//disable timer interrupt
 	P1IE |= HALL;			//re-enable Port Interrupt and
 	IE1 |= WDTIE;			//return control to WDT to get the next output.
	}
}

/*
 * This function sets up hardware interrupt for Hall Sensor analog output
 * Pin 1.0 is connect to Hall sensor output which has a high to low transition when magnet is close to the sensor
 * The port interrupt has a internal pull-up resistor
 */
void set_Hall(void){
	P1DIR &= ~(HALL); // Enable buttons as input.
 	P1REN |= HALL;	// Enable internal pull-up/down resistors for buttons.
 	P1OUT |= HALL;	// Select pull-up mode for buttons (default state is high).
 	P1IES |= HALL;	// Interrupt enable on falling edge.
	P1IE |= HALL; 	// Enable button interrupts.
	P1IFG &= 0;      	// Clear buttons IFG flag.
}

/*
 * This function sets up LED with P1.2 and P1.4 corrresponding to SPI data and CLK
 * SPI is configured to operate in 3-PIN 8-BIT SPI master mode.
 */
void set_led(void){
	P1SEL  =   LED;
	P1SEL2 =   LED;
	UCA0CTL1   =   UCSWRST;
	UCA0CTL0   |=  UCCKPH + UCMODE_0 +UCMSB   +   UCMST   +   UCSYNC; //  3-pin,  8-bit   SPI master
	UCA0CTL1   |=  UCSSEL_2;   //  SMCLK
	UCA0CTL1   &=  ~UCSWRST;   //  **Initialize	USCI	state   machine
}

