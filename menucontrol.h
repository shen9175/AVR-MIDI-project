//***********************************************************************************
//*****FUNCTIONS FOR GENERAL MENU CONTRAL AND SPECIFIC MENU FUNCTIONS  **************
//***********************************************************************************
//				Controller: ATmega644 (Clock: 8 Mhz-internal)
//			Compiler: AVR-GCC (Ubuntu cross-compiler avr-g++ -v4.8.2)
//***********************************************************************************
#ifndef MENUCONTROL_H
#define MENUCONTROL_H

typedef struct
{
  unsigned char* content;
  unsigned char type;	//type=0 is string in the PGM, type=1 is the string in the Data memory
}stringtype;

typedef struct
{
  stringtype name;
  void (*next)();
}menu_t;


extern const unsigned char arrow[];
extern const unsigned char space[];

void menu_control(menu_t* menu, unsigned int number);
void display_back(stringtype var1,stringtype var2);
void display_back(PGM_P var1,PGM_P var2);
void display_back(unsigned char* var1,unsigned char* var2);
void display_continue(stringtype var1,stringtype var2);
void display_continue(PGM_P var1,PGM_P var2);
void main_menu();
void record_menu();
void play_menu();
void sd_menu();
void display_mem();
void list_sd();
void del_file();
void start_record();
void start_play();
#endif
//******** END ***********
