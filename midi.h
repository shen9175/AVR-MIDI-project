//***********************************************************************************
//*****FUNCTIONS FOR MIDI RECEIVE, PLAY, SOUND&FREQUENCY SYNTHESIZATION**************
//***********************************************************************************
//				Controller: ATmega644 (Clock: 8 Mhz-internal)
//			Compiler: AVR-GCC (Ubuntu cross-compiler avr-g++ -v4.8.2)
//***********************************************************************************


#ifndef _MIDI_H_
#define _MIDI_H_

#define fosc 8000000
#define BAUD 31250
#define BAUD_PRESCALE 15

#define CLK 8000000
#define PSCALER 8

//BAUD_PRESCALE=8,000,000/16/31250-1=256/16-1=16-1=15

extern double keys[88];
extern double ocr[88];
extern volatile unsigned long time_count;
extern unsigned char press_f; // press = 1, no press = 0.

/* music note frequency generator */
void generator(void);
void init_sound(void);
void delay (unsigned int dly);
void USART_Init( unsigned int baud_rate);
int roundfloat(double n);
void TIM16_WriteOCR1A(unsigned int i);
ISR(TIMER1_COMPA_vect);
void initialize(void);

unsigned char DeltaTimeConversion(unsigned long ct,unsigned char flag, unsigned char* deltatime);
unsigned long Rounding(double n);


#endif
//******** END ***********
