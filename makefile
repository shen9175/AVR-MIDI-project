
##############################################
#       MIDI project makefile				 #
##############################################

sdtest:FAT32.h SD_routines.h SPI_routines.h LCD_driver.h menucontrol.h midi.h SD_main.o FAT32.o SD_routines.o SPI_routines.o LCD_driver.o menu_control.o midi.o
	avr-g++ -o sdtest -mmcu=atmega644 SD_main.o FAT32.o SD_routines.o SPI_routines.o LCD_driver.o menu_control.o midi.o -O3

SD_main.o:FAT32.h SD_routines.h SPI_routines.h LCD_driver.h menucontrol.h FAT32.o SPI_routines.o SD_routines.o menu_control.o LCD_driver.o SD_main.c
	avr-g++ -c -mmcu=atmega644 SD_main.c -O3

FAT32.o:FAT32.h SD_routines.h menucontrol.h LCD_driver.h midi.h FAT32.c 
	avr-g++ -c -mmcu=atmega644 FAT32.c -O3

LCD_driver.o:LCD_driver.h LCD_driver.c
	avr-g++ -c -mmcu=atmega644 LCD_driver.c -O3

menu_control.o:menucontrol.h FAT32.h menu_control.c
	avr-g++ -c -mmcu=atmega644 menu_control.c -O3

SD_routines.o:SD_routines.h SPI_routines.h SD_routines.c
	avr-g++ -c -mmcu=atmega644 SD_routines.c -O3

SPI_routines.o:SPI_routines.h SPI_routines.c
	avr-g++ -c -mmcu=atmega644 SPI_routines.c -O3

midi.o:midi.h midi.c
	avr-g++ -c -mmcu=atmega644 midi.c -O3


