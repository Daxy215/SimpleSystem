#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../io.h"

/**
 * IO Port	Access Type	Purpose
 * 0x60	Read/Write	Data Port
 * 0x64	Read	     Status Register
 * 0x64	Write	    Command Register
 */
#define KEYBOARD_DATA_PORT     0x60
#define KEYBOARD_COMMAND_PORT  0x64

// TODO; This doesn't include every key
extern char scancodes[3][128];

void handleIrq(void);

void sendCommandToKeyboard(u8 command);
void sendDataToKeyboard(u8 data);
u8 getKeyboardData();
u8 getKeyboardResponse();
u8 waitForAck();

/*
 * TODO; Doesn't work? Unless I'm being stupid
 * 0 - ScrollLock
 * 1 - NumberLock
 * 2 - CapsLock
 * 
 * LED states:
 * Note: Other bits may be used in international keyboards for other purposes (e.g. a Japanese keyboard might use bit 4 for a "Kana mode" LED).
 */
void setLeds(u8 ledState);

/**
 * Returns which scancode does the I/O,
 * port currently uses.
 * 
 * 0 - For scan code set 1
 * 1 - For scan code set 2
 * 2 - For scan code set 3
 */
u8 getScanCode();

/**
 * Sets the scancode to be used,
 * by the I/O port
 * 
 * 0 - For scan code set 1
 * 1 - For scan code set 2
 * 2 - For scan code set 3
 */
void setScanCode(u8 scanCode);

#endif