#include "keyboard.h"

#include "../serial/serial.h"

char scancodes[3][128] = {
    // Set 1
    {
        [0x01] = '0',    [0x02] = '1',    [0x03] = '2',    [0x04] = '3',
        [0x05] = '4',    [0x06] = '5',    [0x07] = '6',    [0x08] = '7',
        [0x09] = '8',    [0x0A] = '9',    [0x0B] = '0',    [0x0C] = '-',
        [0x0D] = '=',    [0x0E] = '\b',   [0x0F] = '\t',   [0x10] = 'q',
        [0x11] = 'w',    [0x12] = 'e',    [0x13] = 'r',    [0x14] = 't',
        [0x15] = 'y',    [0x16] = 'u',    [0x17] = 'i',    [0x18] = 'o',
        [0x19] = 'p',    [0x1A] = '[',    [0x1B] = ']',    [0x1C] = '\n',
        [0x1E] = 'a',    [0x1F] = 's',    [0x20] = 'd',    [0x21] = 'f',
        [0x22] = 'g',    [0x23] = 'h',    [0x24] = 'j',    [0x25] = 'k',
        [0x26] = 'l',    [0x27] = ';',    [0x28] = '\'',   [0x29] = '`',
        [0x2B] = '\\',   [0x2C] = 'z',    [0x2D] = 'x',    [0x2E] = 'c',
        [0x2F] = 'v',    [0x30] = 'b',    [0x31] = 'n',    [0x32] = 'm',
        [0x33] = ',',    [0x34] = '.',    [0x35] = '/',    [0x37] = '*',
        [0x39] = ' ',    [0x4A] = '-',    [0x4E] = '+',    [0x4F] = '1',
        [0x50] = '2',    [0x51] = '3',    [0x52] = '0',    [0x53] = '.'
    },
    // Set 2
    {
        [0x16] = '1',    [0x1E] = '2',    [0x26] = '3',    [0x25] = '4',
        [0x2E] = '5',    [0x36] = '6',    [0x3D] = '7',    [0x3E] = '8',
        [0x46] = '9',    [0x45] = '0',    [0x4E] = '-',    [0x55] = '=',
        [0x66] = '\b',   [0x0D] = '\t',   [0x15] = 'q',    [0x1D] = 'w',
        [0x24] = 'e',    [0x2D] = 'r',    [0x2C] = 't',    [0x35] = 'y',
        [0x3C] = 'u',    [0x43] = 'i',    [0x44] = 'o',    [0x4D] = 'p',
        [0x54] = '[',    [0x5B] = ']',    [0x5A] = '\n',   [0x1C] = 'a',
        [0x1B] = 's',    [0x23] = 'd',    [0x2B] = 'f',    [0x34] = 'g',
        [0x33] = 'h',    [0x3B] = 'j',    [0x42] = 'k',    [0x4B] = 'l',
        [0x4C] = ';',    [0x52] = '\'',   [0x0E] = '`',    [0x1A] = 'z',
        [0x22] = 'x',    [0x21] = 'c',    [0x2A] = 'v',    [0x32] = 'b',
        [0x31] = 'n',    [0x3A] = 'm',    [0x41] = ',',    [0x49] = '.',
        [0x4A] = '/',    [0x5D] = '\\',   [0x29] = ' ',    [0x79] = '+',
        [0x7C] = '*',    [0x7B] = '-',    [0x70] = '0',    [0x69] = '1',
        [0x72] = '2',    [0x7A] = '3',    [0x6B] = '4',    [0x73] = '5',
        [0x74] = '6',    [0x6C] = '7',    [0x75] = '8',    [0x7D] = '9',
        [0x71] = '.'
    },
    // Set 3
    {
        [0x02] = '1',    [0x03] = '2',    [0x04] = '3',    [0x05] = '4',
        [0x06] = '5',    [0x07] = '6',    [0x08] = '7',    [0x09] = '8',
        [0x0A] = '9',    [0x0B] = '0',    [0x0C] = '-',    [0x0D] = '=',
        [0x0E] = '\b',   [0x0F] = '\t',   [0x10] = 'q',    [0x11] = 'w',
        [0x12] = 'e',    [0x13] = 'r',    [0x14] = 't',    [0x15] = 'y',
        [0x16] = 'u',    [0x17] = 'i',    [0x18] = 'o',    [0x19] = 'p',
        [0x1A] = '[',    [0x1B] = ']',    [0x1C] = '\n',   [0x1E] = 'a',
        [0x1F] = 's',    [0x20] = 'd',    [0x21] = 'f',    [0x22] = 'g',
        [0x23] = 'h',    [0x24] = 'j',    [0x25] = 'k',    [0x26] = 'l',
        [0x27] = ';',    [0x28] = '\'',   [0x29] = '`',    [0x2C] = 'z',
        [0x2D] = 'x',    [0x2E] = 'c',    [0x2F] = 'v',    [0x30] = 'b',
        [0x31] = 'n',    [0x32] = 'm',    [0x33] = ',',    [0x34] = '.',
        [0x35] = '/',    [0x39] = ' '
    }
};

void handleIrq(void) {
	while (!(inb(KEYBOARD_COMMAND_PORT) & 0x01));
	u8 status = inb(KEYBOARD_COMMAND_PORT);
	
	// If data is from mouse
	if (status & 0x20) {
		return;
	}
	
	u8 scancode = getKeyboardData();
	u8 curScanCode = getScanCode();
	
	char pressedKey = scancodes[curScanCode][scancode & 0x7F];
	
	if (!(scancode & 0x80)) {
		// Released
		//printSerial("You released:");
		//writeHex(pressedKey);
		//writeSerial('\n');
		printf("You released: %c = %d\n", pressedKey, scancode);
	} else {
		// Pressed
		printf("You pressed : %c = %d\n", pressedKey, scancode);
		
		//printSerial("You pressed:");
		//writeHex(pressedKey);
		//writeSerial('\n');
		
		// TODO; Testing
		//volatile unsigned short* video = (unsigned short*)0xB8000;
		//static int f = 0;
		//video[f++] = 0x0200 | pressedKey;
	}
}

void sendCommandToKeyboard(u8 command) {
	while (inb(KEYBOARD_COMMAND_PORT) & 0x02);  // Wait for the controller to be ready
	outb(KEYBOARD_COMMAND_PORT, command);
}

void sendDataToKeyboard(u8 data) {
	while (inb(KEYBOARD_COMMAND_PORT) & 0x02);  // Wait for the controller to be ready
	outb(KEYBOARD_DATA_PORT, data);	
}

u8 getKeyboardData() {
	return inb(KEYBOARD_DATA_PORT);
}

u8 getKeyboardResponse() {
	while (!(inb(KEYBOARD_COMMAND_PORT) & 0x01));  // Wait until data is available
	return inb(KEYBOARD_DATA_PORT);	
}

u8 waitForAck() {
	u8 response;
	
	do {
		response = (int)getKeyboardResponse();
	} while (response == 0xFE);	// Resend
	
	// 0xFA -> ACK
	if (response != 0xFA) {
		printSerial("Keyboard did not ACK\r\n");
		writeHex(response);
		writeSerial('\n');
	}
	
	return response;
}

// TODO; Rewrite this
void setLeds(u8 ledState) {
	sendCommandToKeyboard(0xED);
    
	u8 response = getKeyboardResponse();
    
	// ACK
	if (response == 0xFA) {
		sendDataToKeyboard(ledState);
        
		response = getKeyboardResponse();
        
		//resend
		if (response == 0xFE) {
			// Resend request, so resend the data byte
			sendDataToKeyboard(ledState);
		}
	}
    
	// Resend
	if (response == 0xFE) {
		// Resend request, so resend the command
		sendDataToKeyboard(0xED);
		response = getKeyboardResponse();
	}
}

u8 getScanCode() {
	// Wait until keyboard is ready
	while (!(inb(KEYBOARD_COMMAND_PORT) & 0x01));
	
	/*
	 * 0xFA (ACK) or 0xFE (Resend) if scan code is being set; 0xFA (ACK) then the scan code set number,
	 * or 0xFE (Resend) if you're getting the scancode. If getting the scancode the table indicates the value that identify each set:
	 *
     * Raw	Translated	Use
     * 1	0x43	Scan code set 1
     * 2	0x41	Scan code set 2
     * 3	0x3F	Scan code set 3
	 */
	sendDataToKeyboard(0xF0);
	waitForAck();
	
	/*
	 * Sub-command:
     * Value	Use
     * 0	Get current scan code set
     * 1	Set scan code set 1
     * 2	Set scan code set 2
     * 3	Set scan code set 3
	 */
	
	// We want to get the current scan code:
	sendDataToKeyboard(0x0);
	waitForAck();
	
	u8 response = getKeyboardResponse();
	
	if (response == 0x43) return 1;
	if (response == 0x41) return 2;
	if (response == 0x3F) return 3;
	
	printSerial("[ERROR] [KEYBOARD] [GETSCANCODE] Invalid response: ");
	writeHex(response);
	writeSerial('\n');
	
	return 1;
}

void setScanCode(u8 scanCode) {
	/*
	 * 0xFA (ACK) or 0xFE (Resend) if scan code is being set; 0xFA (ACK) then the scan code set number,
	 * or 0xFE (Resend) if you're getting the scancode. If getting the scancode the table indicates the value that identify each set:
	 *
     * Raw	Translated	Use
     * 1	0x43	Scan code set 1
     * 2	0x41	Scan code set 2
     * 3	0x3F	Scan code set 3
	 */
	sendDataToKeyboard(0xF0);
	waitForAck();
	
	/*
	 * Sub-command:
     * Value	Use
     * 0	Get current scan code set
     * 1	Set scan code set 1
     * 2	Set scan code set 2
     * 3	Set scan code set 3
	 */
	
	if(scanCode > 3) {
		printSerial("[ERROR] [KEYBOARD] [SETSCANCODE] Invalid scan code of: ");
		writeHex(scanCode);
		writeSerial('\n');
		
		return;
	}
	
	// We want to get the current scan code:
	sendDataToKeyboard(scanCode);
	u8 response = waitForAck();
	
	if(response == 0xFA) {
		printSerial("got:\r");
		writeHex(response);
		writeSerial('\n');
	}
}


