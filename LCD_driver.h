//*****************************************************************************
// **** ROUTINES FOR Hantronix HDM24216H-2 2x16 LCD display driver*************
//*****************************************************************************
//Controller: ATmega644 (Clock: 8 Mhz-internal)
//Compiler: AVR-GCC (Ubuntu cross-compiler avr-g++ -v4.8.2)
//*****************************************************************************


#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H
/* Hantronix HDM24216H-2 2x16 LCD display driver*/
/*
This is document for later reference:

//PORTD for LCD ctrl//
Hardwire pin layout:
PORTD2 (onboard number pin 1)--> LCD on board number pin 6 (Enable,Falling edge): PORTD2=0->enable  PORTD2=1->disable
PORTD3 (onboard number pin 2)--> LCD on board number pin 4 (RS):  PORTD3=RS=0->Instruction Regiester  PORTD3=RS=1->Data Register
PORTD4 (onboard number pin 3)--> LCD on board number pin 5 (R/W): PORTD4=R/W=0->Read data from LCD register   PORTD4=R/W=1->write date to LCD Register

//PORTC for LCD Data//
Hardwire pin layout:
register   PORT pin   LCD on board pin
DB0	   PORTC0	7
DB1	   PORTC1	8
DB2	   PORTC2	9
DB3	   PORTC3	10
DB4	   PORTC4	11
x(DB5)?	   x(PORTB5)?	12?---->seems this is not connected?? but it works.
DB6	   PORTC6	13
DB7	   PORTC7	14
*/



#ifndef F_CPU
#define F_CPU 8000000UL	//this is for _delay_ms(us)() use  defined as CPU frequencey: 8Mhz
#endif

#include <stddef.h>
#include <inttypes.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include "menucontrol.h"



//define control bit
#define LCD_enable PORTD2 
#define LCD_RS PORTD3
#define LCD_RW PORTD4

//define data bit
#define LCD_Data PORTC




/*
Define Commands and Instructions:

Function Set command:
(001)(0/1:1->8bit,0->4bit)|(0/1:0->1line,1->2lines)(0/1:0->5x7dots,1->5x11dots)(00)

0011|1000  0x38: 8bit mode, 2-line, 5x7 dots
0011|1100  0x3c: 8bit mode, 2-line, 5x11dots
0011|0000  0x30: 8bit mode, 1-line, 5x7 dots
0011|0100  0x34: 8bit mode, 1-line, 5x11dots

0010|1000  0x28: 4bit mode, 2-line, 5x7 dots
0010|1100  0x2c: 4bit mode, 2-line, 5x11dots
0010|0000  0x20: 4bit mode, 1-line, 5x7 dots
0010|0100  0x24: 4bit mode, 1-line, 5x11dots



Display Control

0000|1(0/1:1->display turn on,0->display turn off)(0/1:1->cursor on,0->cursor off)(0/1:1->cursor blink on,0->cursor blink off)

0000|1111  0x0f: display on, cursor on, cursor blink on
0000|1110  0x0e: display on, cursor on, cursor blink off
0000|1100  0x0c: display on, cursor off,cursor blink off

the rest is useless
0000|1101  cursor off blink on is useless
0000|1000  0x08: display off, the rest useless
the rest useless



Entry Mode
0000|01(0/1:0->cursor moves to right, RAM address increased by1, 1->cursor moves to left, RAM address decreased by 1)(0/1:0->normal, 1->accompanies display shift?)

0000|0110   0x06: from left to right
0000|0100   0x04: from right to left

don't know what is accompanies display shift, so ignore the rest of two

Return Home
0000|001x
0000|0010  0x02 return home
0000|0011  0x03 return home?

Clear Display
0000|0001 0x01 clear Display

Set DDRAM address to original 0
1000|0000 0x80

*/

#define _8BIT_2LINE_5X7  0x38
#define _8BIT_2LINE_5X11 0x3c
#define _8BIT_1LINE_5X7  0x30
#define _8BIT_1LINE_5X11 0x34

#define DCURSOROFF 0x0c	//display on, cursor off
#define DCURSORONB 0x0f //display on, cursor on, blink on
#define DCURSORONF 0x0e //display on, cursor on, blink off

#define ENTRYTORIGHT 0x06
#define ENTRYTOLEFT  0x04

// Clear Display 0000|0001->0x01
#define CLEARSCR 0x01
// Return Home   0000|0010->0x02
#define RETURNHOME 0x02

//Change the cursor and DDRAM address counter to the first row and first column-->Set DDRAM Address 1xxx|xxxx(xxx|xxxx is the DDRAM address)
//first row first column address is 000|0000 so Set first row and first column is 1000|0000-->0x80
#define FIRST_ROW_START 0x80
//second row first column address is 100|0000 so Set second row and first column is 1100|0000-->0xc0
#define SECOND_ROW_START 0xc0

void LCD_command(unsigned char var);
void LCD_senddata(unsigned char var);
void printf_strDM(unsigned char *var);
void printf_str(stringtype var);
void printf_strPGM(PGM_P var);
void LCD_init();


#endif
//******** END ***********

