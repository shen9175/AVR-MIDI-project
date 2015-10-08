# AVR-MIDI-project
AVR ATMega644 chip wih MIDI Recorder and Player

The system is built on ATMega644 chip.

The board with ATMega644 CPU(micro-controller) with Atmel AVR ISA.
ATmega644 uses harvard architecture with 64K Flash memory for code and data storage and 4K memory for calculation
see specifications on http://www.atmel.com/devices/ATMEGA644.aspx

This project is using buttons on the ATmega644 development board,SD card reader, LCD display,
MIDI keyboard input and external speaker connected the ATmega644 micro-controller
to implement a simple electronic pianno system with MIDI recording and playing.

This project is built ground zero up, not using any other libraries due to only 64K Flash ROM to store the binary code.
No operating system, no FAT32 system, no readfile/write file functions. No SD card drivers, no LCD display drivers, no speaker drivers and
no MIDI decoder and encoders in the begining.

All subsystem, operating system, drivers are self implementd including FAT32 file system of saving loading midi files,
midi encoder and decoder system, LCD driver, SD card driver.

The speaker is implement with simple squre wave with timer interupt as specific frequency to generate the sound.
No instrument timbre are implement in this project.

The specific features and functions are follows:

Implement LCD drivers
Implement SD drivers
Implement FAT32 file system and write and load functions
Implement general MIDI 1.0 encoder and decoder functions
Implement speaker functions.
Implement onboard button functions
Implement round-robin based operating system.
Implement multi-level menus with button control traverse user-interface.
Implement display directories/file, play MIDI file, delete MIDI file
Implement pause, play, switch channel of MIDI file.
Implement recording MIDI from MIDI keyboard and the saved MIDI file can be played on Computer with pianno timbre.

