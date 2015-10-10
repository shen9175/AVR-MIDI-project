# AVR-MIDI-project
AVR ATMega644 chip wih MIDI Recorder and Player

watch the demo video on youtube: https://youtu.be/2f1lXY0KVbk

The system is built on ATMega644 micro-controlled.

The board with ATMega644 CPU(micro-controller) with Atmel AVR ISA.
ATmega644 uses harvard architecture with 64K Flash memory for code and data storage and 4K memory for calculation
see specifications on http://www.atmel.com/devices/ATMEGA644.aspx

This project is using buttons on the ATmega644 development board,SD card reader, LCD display,
MIDI keyboard input and external speaker connected the ATmega644 micro-controller
to implement a simple electronic piano system with MIDI recording and playing.

This project is built from ground zero up, not using any other libraries due to only 64K Flash ROM to store the binary code.
No operating system, no FAT32 system, no readfile/write file functions, no SD card drivers, no LCD display drivers, no speaker drivers and no MIDI decoder and encoders in the begining of the project.

All subsystem, operating system, drivers are self implementd including FAT32 file system of saving loading midi files,
midi encoder and decoder system, LCD driver, SD card driver.

The speaker is implement with simple squre wave with micro-controller timer interupt as specific frequency to generate the sound. No instrument timbre are implement in this project.

The specific features and functions are follows:

Implement LCD drivers
Implement SD card reader drivers
Implement FAT32 file system and write and load functions
Implement general MIDI 1.0 encoder and decoder functions
Implement speaker functions.
Implement onboard button functions
Implement round-robin based operating system.
Implement multi-level menus with button control traverse user-interface.
Implement display directories/file, play MIDI file, delete MIDI file
Implement pause, play, switch channel of MIDI file.
Implement recording MIDI from MIDI keyboard and the saved MIDI file can be played on Computer with piano timbre.

The system is built on  AVR-GCC (Ubuntu cross-compiler avr-g++ -v4.8.2)
The ATmega644 is set as 8Mhz frequency.

The SD card driver and FAT32 filesystem are reference the DharmaniTech's SD card drivers and FAT32 implementaion on ATmega32
http://www.dharmanitech.com/2009/01/sd-card-interfacing-with-atmega8-fat32.html

Sanqing Yuan is the co-author of this project.

watch the demo video on youtube: https://youtu.be/2f1lXY0KVbk
