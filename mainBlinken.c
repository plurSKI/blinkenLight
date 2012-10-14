#include <p18cxxx.h>
#include <delays.h>
#include <adc.h>

/************************************************
/* Magic numbers to get the speed right
/* Tweak these to get the speed going properly
/***********************************************/
#define SPEED_CONST 510  
// Analog input multiplier
#define ANALOG_MAG 3 


// States
// Off every other state so it's easy to get to
#define OFF1 0
#define ON1  1
#define OFF2 2
#define ON2  3
#define OFF3 4
#define ON3  5
#define OFF4 6
#define ON4  7
#define OFF5 8
#define ON5  9
#define OFF6 10
#define ON6  11
#define OFF7 12
#define ON7  13
#define OFF8 14
#define ON8  15

// General chip setup
#pragma config WDT = OFF
#pragma config FOSC = HS
#pragma config LVP = OFF


char state = 15;
char lighton = 0;
char toggle = 0;
void InterruptHandlerHigh (void);

union
{
  struct
  {
    unsigned Timeout:1;         //flag to indicate a TMR0 timeout
    unsigned None:7;
  } Bit;
  unsigned char Byte;
} Flags;



void main (void)
{
  int analogInput = 0;
  int temp = 0;
  int pwm = 0;
  int pwm2 = 0;
  int count = 0;
  int count2 = 0;
  int onTime = 10;
  int speed = 0;
  int dir = -1;
  int time = 0;
  int runOnce = 0;
  int lightUsed = 0;
  int fade = 1;
//  TRISB = 0;
  TRISC = 0;
  TRISA = 0xFF;
  PORTC=0xFF;

  Flags.Byte = 0;
  INTCON = 0x20;                //disable global and enable TMR0 interrupt
  INTCON2 = 0x84;               //TMR0 high priority
  RCONbits.IPEN = 1;            //enable priority levels
  TMR0H = 0;                    //clear timer
  TMR0L = 0;                    //clear timer
  T0CON = 0x81;                 //set up timer0 - prescaler 1:8
  INTCONbits.GIEH = 1;          //enable interrupts


  while(1){
    OpenADC(ADC_FOSC_8 & ADC_RIGHT_JUST & ADC_0_TAD,
       ADC_CH0 & ADC_INT_OFF & ADC_VREFPLUS_VDD & ADC_VREFMINUS_VSS,
	   0b1011);
    SetChanADC(ADC_CH0);
    ConvertADC(); // Start conversion
    while( BusyADC() ); // Wait for ADC conversion
    temp = ReadADC(); // Read result and put in temp
    CloseADC();

	// Grab a start state using the LSBs of the temperature
	if(!runOnce){
		speed = temp & 0b00000111;
		state = 2*speed + 1;
		runOnce = 1;
	} 
    temp = temp >> 2; 
   // PORTB = temp;

	// Calculate speed, used for many of the cases below
	speed = temp & 0b00100000;
	if(!speed) speed = 128 - ((temp & 0b01011111) +96);
	else speed = 128 - (temp & 0b01011111);
	if(speed < 1) speed = 1;

	switch(state){
		case OFF1:
		case OFF2:
		case OFF3:
		case OFF4:
		case OFF5:
		case OFF6:
		case OFF7:
		case OFF8:
			PORTCbits.RC6 = 0;
			PORTCbits.RC7 = 0;
			break;
		case ON1: // Brightness power, random HDD
			onTime =0;

			if (   temp & 0b00000010 ) onTime +=1;
			if (   temp & 0b00000100 ) onTime +=2;
			if (   temp & 0b00001000 ) onTime +=4;			
			if ( !(temp & 0b00010000)  && (temp & 0b10000000)) onTime +=8;

			if(count > 20) count = 0;
			
			// PWM the light
			count ++;
			if(onTime >= count){
				PORTCbits.RC6 = 0xFF;
			} else {
				PORTCbits.RC6 = 0;	
			}	

			// Psuedo-random blinking
			if(((pwm++) % (fade+speed/15)) == 0) PORTCbits.RC7 =0xFF; 
			else PORTCbits.RC7 = 0;
			if(pwm % 1000) fade ++; 
			if( fade > 100 + temp) fade = 1;
			break;
		case ON2: // Blink synced
			count ++;
			speed *= 50;
						
			
			if( count > speed) PORTC = 0xFF;
			else PORTC = 0;
			if(count >= 2*speed) count = 0;
			
			
			break;
		case ON3: // Pulse, syncronized
			time ++;
			if(count > 10) count = 0;
			
			// PWM the light
			count ++;
			if(onTime >= count) PORTC = 0xFF;
			else PORTC = 0;
			speed *= 5;

			// If we are at the end of this brightness move 
			// up one or down one brightness level
			if(time >= speed){
				onTime += dir;
				if(onTime > 10){
					onTime = 9;
					dir = -1;
				}
				if(onTime < 0){
					onTime = 1;
					dir = 1;
				}
				time = 0;
			}
			break;
		case ON4:  // Blink alternating
			count ++;
			speed *= 50;
						
			
			if( count > speed){ 
				if(!lightUsed) {
					PORTCbits.RC7 = 0xFF;
					PORTCbits.RC6 = 0;
				} else { 
					PORTCbits.RC6 = 0xFF;
					PORTCbits.RC7 = 0;
				}
			}else{ 
				if(!lightUsed) {
					PORTCbits.RC7 = 0;
					PORTCbits.RC6 = 0xFF;
				} else { 
					PORTCbits.RC6 = 0;
					PORTCbits.RC7 = 0xFF;
				}
			}
			if(count >= 2*speed){ 
				count = 0;
				if(!lightUsed) lightUsed = 1;
				else lightUsed = 0;
			}
			
			break;
		case ON5:  // Pulse alternating
			time ++;
			if(count > 10) count = 0;
			
			// PWM the light
			count ++;
			if(onTime >= count){
				if(!lightUsed){
					PORTCbits.RC7 = 0xFF;
					PORTCbits.RC6 = 0;
				}else{
					PORTCbits.RC6 = 0xFF;
					PORTCbits.RC7 = 0;
				}
			} else { 
				PORTC = 0;
			}
			speed *= 5;

			// If we are at the end of this brightness move 
			// up one or down one brightness level
			if(time >= speed){
				onTime += dir;
				if(onTime > 10){
					onTime = 9;
					dir = -1;
				}
				if(onTime < 0){
					if(lightUsed) lightUsed = 0;
					else lightUsed = 1;
					onTime = 1;
					dir = 1;
				}
				time = 0;
			}
			break;
		case ON6:  // Pulse power, random HDD
			time ++;
			if(count > 10) count = 0;
			
			// PWM the light
			count ++;
			if(onTime >= count) PORTCbits.RC6 = 0xFF;
			else PORTCbits.RC6 = 0;
			speed *= 5;

			// If we are at the end of this brightness move 
			// up one or down one brightness level
			if(time >= speed){
				onTime += dir;
				if(onTime > 10){
					onTime = 9;
					dir = -1;
				}
				if(onTime < 0){
					onTime = 1;
					dir = 1;
				}
				time = 0;
			}

			// Psuedo-random blinking
			if(((pwm++) % (fade+speed/15)) == 0) PORTCbits.RC7 =0xFF; 
			else PORTCbits.RC7 = 0;
			if(pwm % 1000) fade ++; 
			if( fade > 100 + temp) fade = 1;
			break;

		case ON7:  // Brightness Both
			onTime =0;

			if (   temp & 0b00000010 ) onTime +=2;
			if (   temp & 0b00000100 ) onTime +=4;
			if (   temp & 0b00001000 ) onTime +=8;			
			if ( !(temp & 0b00010000) && (temp & 0b10000000) ) onTime +=16;
			//onTime -= 10;			

			if(count > 40) count = 0;
			
			// PWM the light
			count ++;
			if(onTime >= count){
				PORTC = 0xFF;
			} else {
				PORTC = 0;	
			}
			break;
		case ON8:  //Light comes on when hot, and another when reall hot

			onTime = 0;
			if ( !(temp & 0b00010000) && (temp & 0b10000000)) {
				onTime = 1;
			 	if(temp & 0b00001000) onTime = 2;
			}
		
			switch(onTime){
				case 0:
					PORTC = 0;
					break;
				case 1:
					PORTCbits.RC7 = 0xFF;
					PORTCbits.RC6 = 0;
					break;
				case 2:
					PORTC = 0xFF;
					break;
			}	
			break;
	}
  }
}


#pragma code InterruptVectorHigh = 0x08
void
InterruptVectorHigh (void)
{
  _asm
    goto InterruptHandlerHigh //jump to interrupt routine
  _endasm
}

#pragma code
#pragma interrupt InterruptHandlerHigh

void
InterruptHandlerHigh ()
{
  if (INTCONbits.TMR0IF)
    {                                   //check for TMR0 overflow
      INTCONbits.TMR0IF = 0;            //clear interrupt flag
      Flags.Bit.Timeout = 1;            //indicate timeout

      if( !toggle && PORTAbits.RA4 ) toggle = 1;
      // The button is up after having been down
      if( toggle && !PORTAbits.RA4 ) { 
		toggle = 0; 
		state ++;
		if(state > ON8) state = OFF1;
	  }
    }
}



