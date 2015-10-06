//**************************************************************
// ****** FUNCTIONS FOR SPI COMMUNICATION *******
//**************************************************************
//Controller: ATmega644 (Clock: 8 Mhz-internal)
//Compiler: AVR-GCC (Ubuntu cross-compiler avr-g++ -v4.8.2)
//**************************************************************

#include <avr/io.h>
#include "SPI_routines.h"
//SPI initialize for SD card
//clock rate: 125Khz
void spi_init(void)
{
  PORTB = 0xEF;
  DDRB  = 0xBF; //MISO line i/p, rest o/p
  SPCR = 0x52; //setup SPI: Master mode, MSB first, SCK phase low, SCK idle low
  SPSR = 0x00;
}

unsigned char SPI_transmit(unsigned char data)
{
  // Start transmission
  SPDR = data;

  // Wait for transmission complete
  while(!(SPSR & (1<<SPIF)));
  data = SPDR;
  
  return(data);
}

unsigned char SPI_receive(void)
{
  unsigned char data;
  // Wait for reception complete
  
  SPDR = 0xff;
  while(!(SPSR & (1<<SPIF)));
  data = SPDR;
  
  // Return data register
  return data;
}

//******** END ***********
