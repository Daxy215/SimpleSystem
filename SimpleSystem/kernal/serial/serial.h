// serial.h
#ifndef SERIAL_H
#define SERIAL_H

#include "../io.h"

u8 initSerial(void);
u8 serialReceived(void);
u8 readSerial(void);
u8 isTransmitEmpty(void);

void writeSerial(u8 a);
void printSerial(const char* str);
void writeHex(u8 a);
void printf(const char* format, ...);

#endif