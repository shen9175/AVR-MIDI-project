//**************************************************************
// ****** MAIN FUNCTION FOR MIDI PROJECT ***********************
//**************************************************************
//Controller: ATmega644 (Clock: 8 Mhz-internal)
//Compiler: AVR-GCC (Ubuntu cross-compiler avr-g++ -v4.8.2)
//**************************************************************


#define F_CPU 8000000UL
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "SPI_routines.h"
#include "SD_routines.h"
#include "FAT32.h"
#include "LCD_driver.h"
#include "menucontrol.h"

const unsigned char sdcardtesting[] PROGMEM ="  SD Card Testing..  ";
const unsigned char sdcardnotdectect[] PROGMEM ="SD card not detected..";
const unsigned char initilfail[] PROGMEM ="Card Initialization failed..";
const unsigned char sdv1_1[] PROGMEM ="Standard Capacity Card";
const unsigned char sdv1_2[] PROGMEM ="(Ver 1.x) Detected!";
const unsigned char sdhc_1[] PROGMEM ="High Capacity Card";
const unsigned char sdhc_2[] PROGMEM ="Detected!";
const unsigned char sdv2_1[] PROGMEM ="Standard Capacity Card";
const unsigned char sdv2_2[] PROGMEM ="(Ver 2.x) Detected!";
const unsigned char unknown_1[] PROGMEM ="Unknown SD Card Detected";
const unsigned char unknown_2[] PROGMEM ="Press OK key to continue";
const unsigned char fat32notfound[] PROGMEM ="FAT32 not found!";

//***********************************************************************************/
//call this routine to initialize LCD and SPI for SD card							 /
//***********************************************************************************/
void init_devices(void)
{
  cli();  //all interrupts disabled
  LCD_init();
  spi_init();
  MCUCR = 0x00;
 TIMSK1 = 0x00; //timer interrupt sources
}

//***********************************************************************************/
//Function: the whole project access point 											 /
//***********************************************************************************/
int main(void)
{
  unsigned char error, FAT32_active;
  unsigned int i;
  unsigned char fileName[13];
  
  init_devices();
  
 begining:

  LCD_command(CLEARSCR);
  LCD_command(FIRST_ROW_START);
  printf_strPGM((PGM_P)sdcardtesting);
  
  cardType = 0;

  for (i=0; i<10; i++)
    {
      error = SD_init();
      if(!error) break;
    }
  
  if(error)
    {
      LCD_command(CLEARSCR);
      LCD_command(FIRST_ROW_START);
      if(error == 1) printf_strPGM((PGM_P)sdcardnotdectect);
      if(error == 2) printf_strPGM((PGM_P)initilfail);
      LCD_command(SECOND_ROW_START);
      printf_strDM((unsigned char*)"Press CONFIRM to recheck");
      while(1) //press confirm to continue
	{
	  if(PINA==0xdf)
	    {
	      while(PINA!=0xff);
	      
	      goto begining; //retesting for SD card initialization
	    }
	}
    }
  
  switch (cardType)
    {
    case 1:
      display_continue((PGM_P)sdv1_1,(PGM_P)sdv1_2);
      //printf_str("Standard Capacity Card (Ver 1.x) Detected!");
      break;
    case 2:
      display_continue((PGM_P)(sdhc_1), (PGM_P)(sdhc_2));
      //printf_str("High Capacity Card Detected!");
      break;
    case 3:
      display_continue((PGM_P)(sdv2_1),(PGM_P)(sdv2_2));
      //printf_str("Standard Capacity Card (Ver 2.x) Detected!");
      break;
    default:display_continue((PGM_P)(unknown_1),(PGM_P)(unknown_2));
      break; 
    }
    
  SPI_HIGH_SPEED;	//SCK - 4 MHz
  _delay_ms(1);   //some delay
  
  FAT32_active = 1;
  error = getBootSectorData (); //read boot sector and keep necessary data in global variables
  
  if(error) 	
    {
      LCD_command(CLEARSCR);
      LCD_command(FIRST_ROW_START);
      printf_strPGM((PGM_P)(fat32notfound));  //FAT32 incompatible drive
      FAT32_active = 0;
      LCD_command(SECOND_ROW_START);
      printf_strDM((unsigned char*)"Press CONFIRM to recheck");
      while(1) //press confirm to continue
	{
	  if(PINA==0xdf)
	    {
	      while(PINA!=0xff);
	      
	      goto begining; //retesting for SD card initialization
	    }
	}
    }
  
  while(1)
    main_menu();  //main menu control infinite loop
  
  return 0;
}
//********** END ***********
