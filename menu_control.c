//***********************************************************************************
//*****FUNCTIONS FOR GENERAL MENU CONTRAL AND SPECIFIC MENU FUNCTIONS  **************
//***********************************************************************************
//				Controller: ATmega644 (Clock: 8 Mhz-internal)
//			Compiler: AVR-GCC (Ubuntu cross-compiler avr-g++ -v4.8.2)
//***********************************************************************************


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "LCD_driver.h"
#include "menucontrol.h"
#include "FAT32.h"
using namespace std;

const unsigned char arrow[] PROGMEM ="  -->";
const unsigned char space[] PROGMEM ="     ";
const unsigned char record[] PROGMEM ="Record";
const unsigned char play[] PROGMEM ="Play";
const unsigned char sdcard[] PROGMEM ="SD Card";
const unsigned char nowrecording[] PROGMEM ="Now Recording...";
const unsigned char nowplaying[] PROGMEM ="Now Playing...";
const unsigned char listsdcard[] PROGMEM ="List SD card";
const unsigned char deletefile[] PROGMEM ="Delete file";
const unsigned char sdcardmem[] PROGMEM ="SD card MEM Statistics";
const unsigned char sprintfarg[] PROGMEM =" the file: %s ?";

//***********************************************************************************/
//		Genral Menu Contral function framework										//
//		intput: menu is menu_t struct poiter which include menu string, menu type	//
//		and menu function pointer. number is the number of menu notes				//
//***********************************************************************************/
void menu_control(menu_t* menu, unsigned int number)
{
  //initialize the menu
  unsigned int current=0;
  uint8_t row=1;
  LCD_command(CLEARSCR);
  LCD_command(FIRST_ROW_START);
  printf_strPGM((PGM_P)(arrow));
  printf_str(menu[current].name);
  if(current+1<=number-1)
    {
      LCD_command(SECOND_ROW_START);
      printf_strPGM((PGM_P)(space));
      printf_str(menu[current+1].name);
    }
  
  //wait for input to change the menu
  DDRA=0x00; //set PORTA as input
  while(1)
    {
      switch(PINA)
	{
	case 0x7f://~0x80 sw7 is pushed down, go down
	  {
	    while(PINA!=0xff);//wait the button is released
	    if(current!=number-1)//the current selection is not at the bottom of the menu
	      {
		if(row==1)//arrow in the first row then change it to point second row
		  {
		    row=2;
		    LCD_command(CLEARSCR);
		    printf_strPGM((PGM_P)(space));
		    printf_str(menu[current].name);
		    LCD_command(SECOND_ROW_START);
		    printf_strPGM((PGM_P)(arrow));
		    printf_str(menu[current+1].name);
		  }
		else //row=2->arrow in the second row then scroll the menu down on the screen
		  {
		    row=1;
		    LCD_command(CLEARSCR);
		    printf_strPGM((PGM_P)(arrow));
		    printf_str(menu[current+1].name);
		    if(current+2<=number-1)
		      {
			LCD_command(SECOND_ROW_START);
			printf_strPGM((PGM_P)(space));
			printf_str(menu[current+2].name);
		      }
		  }
		current++;
	      }
	    break;
	  }
	  
	case 0xbf://~0x40 sw6 is push down, go up
	  {
	    while(PINA!=0xff);//wait the button is released
	    if(current!=0)
	      {
		if(row=2)//arrow in the second row then change it to point the first row
		  {
		    row=1;
		    LCD_command(CLEARSCR);
		    printf_strPGM((PGM_P)(arrow));
		    printf_str(menu[current-1].name);
		    LCD_command(SECOND_ROW_START);
		    printf_strPGM((PGM_P)(space));
		    printf_str(menu[current].name);			
		  }
		else //row=1->arrow in the first row then scroll the menu up on the screen
		  {
		    row=2;
		    LCD_command(CLEARSCR);
		    LCD_command(SECOND_ROW_START);
		    printf_strPGM((PGM_P)(arrow));
		    printf_str(menu[current-1].name);
		    if(current-2>=0)
		      {
			LCD_command(FIRST_ROW_START);
			printf_strPGM((PGM_P)(space));
			printf_str(menu[current-2].name);
		      }
		  }
		current--;	
	      }
	    break;
	  }
	  
	case 0xdf://~0x20 sw5 is pushed down-->confirm selection
	  {
	    while(PINA!=0xff);//wait the button is released
	    if(menu[current].next!=NULL)
	      {
			menu[current].next();
			current=0;
			row=1;
			LCD_command(CLEARSCR);
			LCD_command(FIRST_ROW_START);
			printf_strPGM((PGM_P)(arrow));
			printf_str(menu[current].name);
			if(current+1<=number-1)
			  {
			    LCD_command(SECOND_ROW_START);
			    printf_strPGM((PGM_P)(space));
			    printf_str(menu[current+1].name);
			  }
	      }
	    break;
	  }
	  
	case 0xef://~0x10 sw4 is pushed down-->go back
	  {
	    while(PINA!=0xff);//wait the button is released
	    
	    return;
	    break;
	  }
	  	  
	default:
	  break;
	  
	}//end of switch
    }//end of while(1)
}


//***********************************************************************************/
// 		Display two lines of message and press BACK key to go back					//
//		This function is for two type of string: one is stored in Data memory		//
//		The other type is the string stored in PROGAM memory						//
//		stringtype is a struct that have these two kinds of string					//
//**********************************************************************************//
void display_back(stringtype var1,stringtype var2)
{
  LCD_command(CLEARSCR);
  LCD_command(FIRST_ROW_START);
  printf_str(var1);
  LCD_command(SECOND_ROW_START);
  printf_str(var2);
  
  DDRA=0x00; //set PORTA as input
  while(1)
    {
      switch(PINA)
	{
	case 0xef://~0x10 sw4 is pushed down-->go back
	  {
	    while(PINA!=0xff);//wait the button is released
	    return;
	    break;
	  }
	  
	default:
	  break;
	}
    }
}

//***********************************************************************************/
// 		Display two lines of message and press BACK key to go back					//
//		This function is for the string which is stored in 64K PROGAM memory		//
//**********************************************************************************//
void display_back(PGM_P var1,PGM_P var2)
{
  LCD_command(CLEARSCR);
  LCD_command(FIRST_ROW_START);
  printf_strPGM(var1);
  LCD_command(SECOND_ROW_START);
  printf_strPGM(var2);
  
  DDRA=0x00; //set PORTA as input
  while(1)
    {
      switch(PINA)
	{
	case 0xef://~0x10 sw4 is pushed down-->go back
	  {
	    while(PINA!=0xff);//wait the button is released
	    return;
	    break;
	  }
	  
	default:
	  break;
	}
    }
}

//***********************************************************************************/
// 		Display two lines of message and press BACK key to go back					//
//		This function is for the string which is stored in 4K Data memory			//
//**********************************************************************************//
void display_back(unsigned char* var1,unsigned char* var2)
{
  LCD_command(CLEARSCR);
  LCD_command(FIRST_ROW_START);
  printf_strDM(var1);
  LCD_command(SECOND_ROW_START);
  printf_strDM(var2);
  
  DDRA=0x00; //set PORTA as input
  while(1)
    {
      switch(PINA)
	{
	case 0xef://~0x10 sw4 is pushed down-->go back
	  {
	    while(PINA!=0xff);//wait the button is released
	    return;
	    break;
	  }
	  
	default:
	  break;
	}
    }
}

//***********************************************************************************/
// 		Display two lines of message and press CONFIRM key to continue				//
//		This function is for two type of string: one is stored in Data memory		//
//		The other type is the string stored in PROGAM memory						//
//		stringtype is a struct that have these two kinds of string					//
//**********************************************************************************//
void display_continue(stringtype var1,stringtype var2)
{
  LCD_command(CLEARSCR);
  LCD_command(FIRST_ROW_START);
  printf_str(var1);
  LCD_command(SECOND_ROW_START);
  printf_str(var2);
  
  DDRA=0x00; //set PORTA as input
  while(1)
    {
      switch(PINA)
	{
	case 0xdf://~0x10 sw5 is pushed down-->confirm selection
	  {
	    while(PINA!=0xff);//wait the button is released
	    return;
	    break;
	  }
	  
	default:
	  break;
	}
    }
}


//***********************************************************************************/
// 		Display two lines of message and press CONFIRM key to continue				//
//		This function is for the string which is stored in 64K PROGAM memory		//
//**********************************************************************************//
void display_continue(PGM_P var1,PGM_P var2)
{
  LCD_command(CLEARSCR);
  LCD_command(FIRST_ROW_START);
  printf_strPGM(var1);
  LCD_command(SECOND_ROW_START);
  printf_strPGM(var2);
  
  DDRA=0x00; //set PORTA as input
  while(1)
    {
      switch(PINA)
	{
	case 0xdf://~0x10 sw5 is pushed down-->confirm selection
	  {
	    while(PINA!=0xff);//wait the button is released
	    return;
	    break;
	  }
	  
	default:
	  break;
	}
    }
}

//***********************************************************************************/
//			The main menu and the whole menu system access point					//
//***********************************************************************************/
void main_menu()
{
  menu_t main_menu[3];
  main_menu[0].name.content=(unsigned char*)(PGM_P)(record);
  main_menu[0].name.type=0;
  main_menu[0].next=start_record;//record_menu;
  main_menu[1].name.content=(unsigned char*)(PGM_P)(play);
  main_menu[1].name.type=0;
  main_menu[1].next=start_play;
  main_menu[2].name.content=(unsigned char*)(PGM_P)(sdcard);
  main_menu[2].name.type=0;
  main_menu[2].next=sd_menu;
  menu_control(main_menu,3);
}

//***********************************************************************************/
//			The SD card management menu function									//
//***********************************************************************************/
void sd_menu()
{
  menu_t sd_menu[3];
  sd_menu[0].name.content=(unsigned char*)(PGM_P)(listsdcard);
  sd_menu[0].name.type=0;
  sd_menu[0].next=list_sd;
  sd_menu[1].name.content=(unsigned char*)(PGM_P)(deletefile);
  sd_menu[1].name.type=0;
  sd_menu[1].next=del_file;
  sd_menu[2].name.content=(unsigned char*)(PGM_P)(sdcardmem);
  sd_menu[2].name.type=0;
  sd_menu[2].next=display_mem;
  menu_control(sd_menu,3);
}

//***********************************************************************************/
//			The recording function access point										//
//***********************************************************************************/
void start_record()
{
  unsigned char newfileName[] = "Record01.mid";
  createName(newfileName);
  writeFile(newfileName);
 }

//***********************************************************************************/
//			The Playing function access point										//
//***********************************************************************************/
void start_play()
{
  listFiles(PLAY,rootCluster);
}

//***********************************************************************************/
//			The SD card statistic function access point								//
//***********************************************************************************/
void display_mem()
{
  menu_t mem_display[4];
  memoryStatistics(mem_display);
  menu_control(mem_display,4);
}

//***********************************************************************************/
//			The List File function access point										//
//***********************************************************************************/

void list_sd()
{
  listFiles(GET_LIST,rootCluster);	
}

//***********************************************************************************/
//			The Delete File function access point									//
//***********************************************************************************/
void del_file()
{
  listFiles(DELETE,rootCluster);
}


//******** END ***********

