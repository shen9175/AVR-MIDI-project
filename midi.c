//***********************************************************************************
//*****FUNCTIONS FOR MIDI RECEIVE, PLAY, SOUND&FREQUENCY SYNTHESIZATION**************
//***********************************************************************************
//				Controller: ATmega644 (Clock: 8 Mhz-internal)
//			Compiler: AVR-GCC (Ubuntu cross-compiler avr-g++ -v4.8.2)
//***********************************************************************************

#include <inttypes.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <math.h>
#include "midi.h"
#include "SD_routines.h"
#include "LCD_driver.h"

extern unsigned char buffer_m[3];

double keys[88];
double ocr[88];

/***************************************************************************************************/
/*			 		88 piano keys of music note frequency generator							   		/
/***************************************************************************************************/
void generator(void)
{
  keys[0]=27.50;
  int i;
  
  for(i=0;i<88;i++)
    {
    if(i>0)
      keys[i]=keys[i-1]*pow(2.0,1/12.0);
    ocr[i]=CLK/(2*PSCALER*keys[i])-1;
    }
}

/***************************************************************************************************/
//				 initial timer interrupt for sound synthesization								   //
/***************************************************************************************************/
void init_sound(void)
{
  DDRD |= (1<<PORTD6);
  PORTD |= (1<<PORTD6);			// speaker toggle target PORTD pin 6
  TCCR1B|= (1<<WGM12)|(1<<CS11);   // OCR1A as top value CTC mode disconnect OC1A/B and 1/8 prescaler
  TIFR1|= (1<<OCF1A);
  TIMSK1|= (1<<OCIE1A);
  generator();
}


/***************************************************************************************************/
// enable USART with baud_rate Baud rate 															/
// input: baud_rate is 16 bit unsigned int for USART Baud rate set up								/
/***************************************************************************************************/
void USART_Init( unsigned int baud_rate)
{
  /* Set baud rate */	
  UBRR0L = (unsigned char) baud_rate;
  /* Enable receiver and transmitter */
  UCSR0B = (1<<RXEN0)|(1<<TXEN0);
  /* Set frame format: 8data */
  
  UCSR0C = 0x86;
  UBRR0H = 0x00;	
}

/***************************************************************************************************/
// floating double number into signed int 2 bytes 16-bit											/
// input: double precision number  output: 2 bytes 16-bit signed int								/
/***************************************************************************************************/
int roundfloat(double n)
{
  if(n-(int)n>=0.5)
    return (int) (n+1);
  else
    return (int) n;
}


/***************************************************************************************************/
// floating double number into unsigned long int 4 bytes 32bit										/
// input: double precision number  output: 4 bytes 32-bit unsigned long int							/
/***************************************************************************************************/
unsigned long Rounding(double n)
{
  if(n-(unsigned long)n>=0.5)
    return (unsigned long)n+1;
  else
    return (unsigned long)n;
}

/***************************************************************************************************/
//							    Change 16bit OCR value							  					/
/***************************************************************************************************/
void TIM16_WriteOCR1A(unsigned int i)
{
  uint8_t sreg;
  sreg=SREG;
  cli();
  OCR1A=i;
  SREG=sreg;
  sei();
}

/***************************************************************************************************/
// 16 bit Timer interrupt handler for processing play and non-play									/
// when press_f==1, play sound, when press_f!=1 sound mute for certain time							/
/***************************************************************************************************/
ISR(TIMER1_COMPA_vect)
{
  if(press_f)
    {
      time_count++;
      PORTD^=1<<PORTD6;    // Toggle pins on Port D
    }
  else
    time_count++;
}

/***************************************************************************************************/
// The general iniliazation function for sound, USART,MIDI file initialization					   //
/***************************************************************************************************/
void initialize(void)
{
  DDRB=0xff;
  PORTB=0xff;
  init_sound();	
  USART_Init(BAUD_PRESCALE);
  unsigned char buffer_temp[] = {'M', 'T', 'h', 'd', 0, 0, 0, 0x06, 0, 1, 0, 3, 0x00, 0x60, 
				 'M', 'T', 'r', 'k', 0, 0, 0, 0x19, 0, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20, 0, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08, 0, 0xFF, 0x59, 0x02, 0, 0, 0, 0xFF, 0x2F, 0,
				 'M', 'T', 'r', 'k', 0, 0, 0, 0x1B, 0, 0xFF, 0x02, 0x13, 'S', 'H', 'E', 'H', ',', 'Y', 'U', 'A', 'N', ',', 'R', 'I', 'N', 'K', 'E', 'R', ',', 'U', 'I', 0, 0xFF, 0x2F, 0,
				 'M', 'T', 'r', 'k', 0, 0, 0, 0,    0, 0xFF, 0x20, 0x01, 0, 0, 0xC0, 0x01};
  int i;
  for(i=0;i<98;++i)
    buffer[i]=buffer_temp[i];
}

/***************************************************************************************************/
// The kernal function for MIDI file that convert real playing time to MIDI file deltatime
// input: ct is the real playing time, flag is for play&unplay, 
//	deltatime is produced MIDI file deltatime (at most 4 bytes)
// output: how many bytes of the deltatime
/***************************************************************************************************/
unsigned char DeltaTimeConversion(unsigned long ct,unsigned char flag, unsigned char* deltatime)
{
  double time_d;
  if(flag)//flag==1  midi keyborad's key is pressed
    time_d=ct/(2*keys[buffer_m[1]-21]);
  else
    time_d=ct/2000.0;
  unsigned long rv;
  unsigned long time;
  //time=Rounding(time_d/0.03125)*0.03125;//0.03125=0.5/16   define 0.5s as one quater note, 0.5/16 as 1/64 note time in second
  
  //time=Rounding(time*96.0/16.0);
  
  time=Rounding(time_d*32)*6; //6=192/32  time is delta time
  
  unsigned char counter=1;
  if(time>127)//2**(7x1)-1
    {
      counter++;
      if(time>16383)//2**(2x7)-1
	{
	  counter++;
	  if(time>2097151)//2**(3x7)-1
	    {
	      counter++;
	    }
	}
    }
  
  switch(counter)
    {
    case 1:
      {
	rv=time;//do nothing
	deltatime[0]=rv&0x000000ff;
	deltatime[1]=0;
	deltatime[2]=0;
	deltatime[3]=0;
      }
      break;
    case 2:
      {
	rv=0x00008000+((time&0x00003f80)<<1)+(0x0000007f&time);
	deltatime[0]=(rv&0x0000ff00)>>8;
	deltatime[1]=rv&0x000000ff;
	deltatime[2]=0;
	deltatime[3]=0;
      }
      break;
    case 3:
      {
	rv=0x00800000+((time&0x001fc000)<<2)+(0x00008000+((time&0x00003f80)<<1)+(0x0000007f&time));
	deltatime[0]=(rv&0x00ff0000)>>16;
	deltatime[1]=(rv&0x0000ff00)>>8;
	deltatime[2]=rv&0x000000ff;
	deltatime[3]=0;
      }
      break;
    case 4:
      {
	rv=0x80000000+((time&0x0fe00000)<<3)+(0x00800000+((time&0x001fc000)<<2)+(0x00008000+((time&0x00003f80)<<1)+(0x0000007f&time)));
	deltatime[0]=(rv&0xff000000)>>24;
	deltatime[1]=(rv&0x00ff0000)>>16;
	deltatime[2]=(rv&0x0000ff00)>>8;
	deltatime[3]=rv&0x000000ff;
      }
      break;
    default:
      rv=0;
      break;
    }
  
  return counter;
}
//******** END ***********
