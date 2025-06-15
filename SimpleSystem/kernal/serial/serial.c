#include "serial.h"

#include <stdarg.h>

// Serial memory start location 0x3F8   - https://wiki.osdev.org/Memory_Map_(x86)#BIOS_Data_Area_.28BDA.29

/**
  * COM Port	IO Port
  * COM1	0x3F8
  * COM2	0x2F8
  * COM3	0x3E8
  * COM4	0x2E8
  * COM5	0x5F8
  * COM6	0x4F8
  * COM7	0x5E8
  * COM8	0x4E8
  */
#define PORT 0x3f8    // COM1

#ifndef NULL
#define NULL ((void*)0)
#endif

// https://wiki.osdev.org/Serial_Ports#Example_Code
u8 initSerial() {
    outb(PORT + 1, 0x00);    // Disable all interrupts
    outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(PORT + 1, 0x00);    //                  (hi byte)
    outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    outb(PORT + 4, 0x1E);    // Set in loopback mode, test the serial chip
    outb(PORT + 0, 0xAE);    // Test serial chip (send byte 0xAE and check if serial returns same byte)
    
    // Check if serial is faulty (i.e: not same byte as sent)
    if(inb(PORT + 0) != 0xAE) {
        return 1;
    }
    
    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(PORT + 4, 0x0F);
    
    return 0;
}

// Receiving data
u8 serialReceived() {
    return inb(PORT + 5) & 1;
}

u8 readRerial() {
    while (serialReceived() == 0);
    
    return inb(PORT);
}

// Sending data
u8 isTransmitEmpty() {
    return inb(PORT + 5) & 0x20;
}

void writeSerial(u8 a) {
    // Wait until transmission is not empty
    while (isTransmitEmpty() == 0);
    
    outb(PORT, a);
}

void printSerial(const char* str) {
	for(int i = 0; str[i]; i++) {
		while (isTransmitEmpty() == 0);
		
        writeSerial(str[i]);
    }
}

void writeHex(u8 a) {
    char hex_digits[] = "0123456789ABCDEF";
    
    writeSerial('0');
    writeSerial('x');
    
    // Print the high nibble
    writeSerial(hex_digits[(a >> 4) & 0x0F]);
    
    // Print the low nibble
    writeSerial(hex_digits[a & 0x0F]);
}

void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    for (const char* p = format; *p != '\0'; p++) {
        if (*p != '%') {
            writeSerial(*p);
            continue;
        }
        
        p++; // Skip '%'
        if (*p == '%') {
            writeSerial('%');
            continue;
        }
        
        switch (*p) {
            case 'c': {
                i8 c = (i8)va_arg(args, int);
                writeSerial(c);
                
                break;
            }
            
            case 's': {
                const char* str = va_arg(args, const char*);
                printSerial(str ? str : "(null)");
                
                break;
            }
            
            case 'x': {
                u32 num = va_arg(args, u32);
                
                writeSerial('0');
                writeSerial('x');
                
                // Special case for 0
                if (num == 0) {
                    writeSerial('0');
                    break;
                }
                
                // Print full 32-bit hex without leading zero suppression
                for (int shift = 28; shift >= 0; shift -= 4) {
                    u8 digit = (num >> shift) & 0xF;
                    writeSerial("0123456789ABCDEF"[digit]);
                }
                
                break;
            }
            
            case 'd': {
                i32 num = va_arg(args, i32);
                
                // Handle edge case
                if (num == -2147483648L) {
                    printSerial("-2147483648");
                    
                    break;
                }
                
                if (num < 0) {
                    writeSerial('-');
                    num = -num;
                }
                
                // Convert number to string
                char buf[12]; // Enough for -2147483648
                int i = 0;
                
                do {
                    buf[i++] = '0' + (num % 10);
                    num /= 10;
                } while (num > 0);
                
                // Print digits in reverse
                while (i-- > 0) {
                    writeSerial(buf[i]);
                }
                
                break;
            }
            
            default: {
                writeSerial('%');
                writeSerial(*p);
                
                break;
            }
        }
    }
    
    va_end(args);
}
