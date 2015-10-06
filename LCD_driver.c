//*****************************************************************************
// **** ROUTINES FOR Hantronix HDM24216H-2 2x16 LCD display driver*************
//*****************************************************************************
//Controller: ATmega644 (Clock: 8 Mhz-internal)
//Compiler: AVR-GCC (Ubuntu cross-compiler avr-g++ -v4.8.2)
//*****************************************************************************
#include "LCD_driver.h"


//***********************************************************************************/
//Function: LCD initiliazation: wait for voltage and clear screen,					 /
//			set display mode to 5x7 dots 8bit mode									 /
//			set cursor to the first row and first column							 /
//***********************************************************************************/
void LCD_init()
{
  DDRC=0xFF;
  DDRD=0x1C;
  _delay_ms(30);//wait 20ms or more after VDD reach 4.5V
  
  LCD_command(_8BIT_2LINE_5X7);   // 2 lines 5x7 dots 8bit mode
  LCD_command(DCURSOROFF);   // display on cursor off  0x0f->Cursor on
  LCD_command(CLEARSCR);   // clear display
  LCD_command(ENTRYTORIGHT);   // entry mode
  LCD_command(FIRST_ROW_START);   // Seeks the pointer to the begining
} 

//***********************************************************************************/
// Function: rountine for send LCD register command									//
// Argument: var the command to be sent to the LCD control chip register			//
//***********************************************************************************/
void LCD_command(unsigned char var)
{
  PORTC=var;
  PORTD=(0<<LCD_RS|0<<LCD_RW|1<<LCD_enable);
  PORTD=(0<<LCD_RS|0<<LCD_RW|0<<LCD_enable);
  switch(var)
    {
    case CLEARSCR:
    case RETURNHOME:
      _delay_us(1640);
      break;
    default:
      _delay_us(40);
      break;
    }
}

//***********************************************************************************/
// Function: rountine for send LCD register data to be displayed					//
// Argument: var the character to be sent to the LCD to be displayed				//
//***********************************************************************************/
void LCD_senddata(unsigned char var)
{
  PORTC=var;
  PORTD=(1<<LCD_RS|0<<LCD_RW|1<<LCD_enable);
  PORTD=(1<<LCD_RS|0<<LCD_RW|0<<LCD_enable);
  _delay_us(46);
}

//***********************************************************************************/
// Function: print function for string from 4K Data memory							//
// Argument: var--the string from 4K Data memory to be printed						//
//***********************************************************************************/
void printf_strDM(unsigned char *var)
{
  unsigned char i;
  for(i=0;i<24;++i)
    {
      if(!var[i])
	break;
      LCD_senddata(var[i]);
    }
}

//***********************************************************************************/
// Function: print function for string from 64K PRGRAM memory						//
// Argument: var--the string from 64K PRGRAM memory to be printed					//
//***********************************************************************************/
void printf_strPGM(PGM_P var)
{
  while(pgm_read_byte(var))
    LCD_senddata(pgm_read_byte(var++));
}

//*******************************************************************************************/
// Function: print function for string from either 4K Data memory or 64K PROGAM memory 		 /
// Argument: var--the string from either 4K Data memory or 64K PROGAM memory to be printed	 /
//*******************************************************************************************/
void printf_str(stringtype var)
{
  if(var.type)//type==1: string in 2k Data memory
    {
      printf_strDM((unsigned char*)var.content);
    }
  else//type==0: string in 32K program memory
    {
      printf_strPGM((PGM_P)var.content);
    }
}
//******** END ***********

