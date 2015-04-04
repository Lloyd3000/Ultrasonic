/*************************************************************************
Title:    Ultrasonic Volume Control
Author:   Elias Fröhlich 
Web: http://hackaday.io/project/2632-Ultrasonic-Volume-Control
**************************************************************************/

#define f_cpu 16000000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "usbdrv.h"

//The USB Hid Report Handler that tells the PC what data to expect
PROGMEM char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {
    0x05, 0x0c,                    // USAGE_PAGE (Consumer Devices)
    0x09, 0x01,                    // USAGE (Consumer Control)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x85, 0x01,                    //   REPORT_ID (1)
    0x19, 0x00,                    //   USAGE_MINIMUM (Unassigned)
    0x2a, 0x3c, 0x02,              //   USAGE_MAXIMUM (AC Format)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0x3c, 0x02,              //   LOGICAL_MAXIMUM (572)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x10,                    //   REPORT_SIZE (16)
    0x81, 0x00,                    //   INPUT (Data,Var,Abs)
    0xc0                           // END_COLLECTION  
};

//Declare some variables
typedef struct {
	uint8_t modifier;
	uint8_t reserved;
	uint8_t keycode[2];
} keyboard_report_t;

static uchar reportBuffer[3] = {0,0,0} ;
volatile int test;
volatile uchar KeyPressed = 0;
static keyboard_report_t keyboard_report; 

static uchar idleRate; 


//set up the USB connection
usbMsgLen_t usbFunctionSetup(uchar data[8]) {
    usbRequest_t *rq = (void *)data;

    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {
        switch(rq->bRequest) {
        case USBRQ_HID_GET_REPORT: // send "no keys pressed" if asked here
            // wValue: ReportType (highbyte), ReportID (lowbyte)
            usbMsgPtr = (void *)&keyboard_report; // we only have this one
            keyboard_report.modifier = 0;
            keyboard_report.keycode[0] = 0;
            return sizeof(keyboard_report);
		case USBRQ_HID_SET_REPORT: // if wLength == 1, should be LED state
            return (rq->wLength.word == 1) ? USB_NO_MSG : 0;
        case USBRQ_HID_GET_IDLE: // send idle rate to PC as required by spec
            usbMsgPtr = &idleRate;
            return 1;
        case USBRQ_HID_SET_IDLE: // save idle rate as required by spec
            idleRate = rq->wValue.bytes[1];
            return 0;
        }
    }
    
    return 0; // by default don't return any data
}


int main() {

	unsigned int count;
	
	int i;
	
	unsigned char  set = 0, send = 0, on = 1, times = 1;
	
	DDRB = 0b00001110; // PB1,2,3 as output rest is input
	PORTB |= 1 << PB0; // PB0 is input with internal pullup resistor activated
	PORTB |= 1 << PB4; // PB4 is input with internal pullup resistor activated    
    wdt_enable(WDTO_1S); // enable 1s watchdog timer

	PORTB |= (1 << PB2); //LED1 on
	PORTB &= ~(1 << PB1);  //LED2 off
			
    usbInit();
	
    usbDeviceDisconnect(); // enforce re-enumeration
    for(i = 0; i<250; i++) { // wait 500 ms
        wdt_reset(); // keep the watchdog happy
        _delay_ms(2);
    }
	
    usbDeviceConnect();
	
	reportBuffer[0] = 1;  // ReportID = 1
	reportBuffer[2] = 0;  
	
  
    sei(); // Enable interrupts after re-enumeration
	
    while(1) {
        wdt_reset(); // keep the watchdog happy
        usbPoll();

			
		if(!(PINB & (1<<PB0))) { // button pressed (PB1 at ground voltage)
			
			 if(on == 1){
			on = 0;
			PORTB &= ~(1 << PB2); // Triggerline turn on	
			PORTB |= (1 << PB1); 
			}
			else{
			PORTB |= (1 << PB2); 
			PORTB &= ~(1 << PB1); // Triggerline turn on	
			on = 1;
			}
		
			for(i = 0; i<250; i++) { // wait 500 ms
			usbPoll();
			wdt_reset(); // keep the watchdog happy
			_delay_ms(2);
			} 
			
		}


		if(on == 1){
		
		
		//Sending th trig signal
		PORTB |= (1 << PB3); 
		_delay_us(10);
		PORTB &= ~(1 << PB3);


		//Polling the Input pin
		for(i = 0; i<1250; i++) { 
		
			wdt_reset(); // keep the watchdog happy
			usbPoll();
	
			if((PINB & (1<<PB4))){
			count ++;

			}else{

			}
			_delay_us(5);
		}	



		//what key is pressed?
		
		if((count <= 80)&(set == 0)){
		
			KeyPressed = 0xCD;
			send = 1;
			times = 1;
			set = 1;
			
		}else if((count >= 81) & (count <= 140)){

			KeyPressed = 0xEA;
			send = 1;
			times = 10;
			
		}else if((count >= 140) & (count <= 258)){

			KeyPressed = 0xE9;
			send = 1;
			times = 10;
			
		}else{

			set = 0;
		
		} 
		
		
	//Send the KEy
		if((send == 1)){
		

			
		while(times>=1){
				usbPoll();
				wdt_reset(); // keep the watchdog happy		
			times --;
			
			reportBuffer[1] = KeyPressed;
			usbSetInterrupt(reportBuffer, sizeof(reportBuffer));
			
			_delay_ms(10); //	
							 			
			reportBuffer[1] = 0x00;
			usbSetInterrupt(reportBuffer, sizeof(reportBuffer));
			
			_delay_ms(10); //		 
		}

		PORTB |= (1 << PB1); //LED on
		PORTB |= (1 << PB2); //LED on

			for(i = 0; i<150; i++) { // wait 300 ms
				usbPoll();
				wdt_reset(); // keep the watchdog happy
				_delay_ms(2);
			}
			
			PORTB &= ~(1 << PB1); // Triggerline turn on	
			PORTB |= (1 << PB2); 		
			
		send = 0;
		
		}
		
		}
		
		
		count = 0;
		

		
		
    }
	
    return 0;
}

