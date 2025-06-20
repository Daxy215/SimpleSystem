#ifndef IO_H
#define IO_H

// Loops timeout
#define TIMEOUT 100000

typedef unsigned char      u8 ;
typedef          char      i8 ;

typedef unsigned short     u16;
typedef          short     i16;

typedef unsigned long      u32;
typedef signed   long      s32;
typedef          long      i32;

#ifdef _WIN64
    typedef unsigned __int64 size_t;
    typedef __int64          ptrdiff_t;
    typedef __int64          intptr_t;
#else
    typedef unsigned int     size_t;
    typedef int              ptrdiff_t;
    typedef int              intptr_t;
#endif

#ifndef NULL
    #ifdef __cplusplus
        #define NULL 0
    #else
        #define NULL ((void *)0)
    #endif
#endif

#if __WORDSIZE == 64
# ifndef __intptr_t_defined
typedef long int		intptr_t;
#  define __intptr_t_defined
# endif
typedef unsigned long int	uintptr_t;
#else
# ifndef __intptr_t_defined
typedef int			intptr_t;
#  define __intptr_t_defined
# endif
# endif

typedef unsigned int		uintptr_t;

typedef void (*irq_handler_t)(void);

//volatile unsigned short* video = (unsigned short*)0xB8000;

// https://wiki.osdev.org/Inline_Assembly/Examples

static inline void outb(unsigned short port, u8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(unsigned short port, u32 val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outsw(u16 port, const void *addr, u32 count) {
    asm volatile (
        "rep outsw"
        : "+S"(addr), "+c"(count)
        : "d"(port)
    );
}

static inline u8 inb(unsigned short port) {
    u8 ret;
    
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    
    return ret;
}

static inline void insw(u16 port, void* addr, u32 count) {
    asm volatile (
        "rep insw"
        : "+D"(addr), "+c"(count)
        : "d"(port)
        : "memory"
    );
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

#include "serial/serial.h"
#include <stdbool.h>

static inline bool lba_read(u32 lba, u32 count, void* buffer) {
    if (count <= 255) {
        outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
        outb(0x1F2, count & 0xFF);
        outb(0x1F3, lba & 0xFF);
        outb(0x1F4, (lba >> 8) & 0xFF);
        outb(0x1F5, (lba >> 16) & 0xFF);
        outb(0x1F7, 0x20);  // READ SECTORS
        
        for (u32 i = 0; i < count; i++) {
            u32 wait_count = 0;
            u8 status;
            
            while (1) {
                status = inb(0x1F7);
                
                // Handle errors first
                if (status & 0x01) {
                    u8 error = inb(0x1F1);
                    
                    printf("ATA error: %x\n", error);
                    
                    return false;
                }
                
                // Check ready state
                if (!(status & 0x80) && (status & 0x08)) {
                    break; // Ready for transfer
                }
                
                if (++wait_count > 1000000) {
                    printf("Timeout: status=%x\n", status);
                    
                    return false;
                }
                
                asm volatile ("pause");
            }
            
            insw(0x1F0, buffer, 256);
            buffer = (void*)((u32)buffer + 512);
        }
        
        return true;
    }
    
    // Large read handling
    while (count > 0) {
        u8 sectors = (count > 255) ? 255 : count;
        
        if (!lba_read(lba, sectors, buffer)) {
            return false;
        }
        
        lba += sectors;
        count -= sectors;
        buffer = (void*)((u32)buffer + sectors * 512);
    }
    
    return true;
}

// TODO; Update this too
static inline void lba_write(u32 lba, u32 count, const void* buffer) {
    while (count > 0) {
        u8 sectors_to_write = (count > 255) ? 255 : count & 0xFF;
        
        outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
        outb(0x1F2, sectors_to_write);               // Write up to 255 sectors
        outb(0x1F3, lba & 0xFF);
        outb(0x1F4, (lba >> 8) & 0xFF);
        outb(0x1F5, (lba >> 16) & 0xFF);
        outb(0x1F7, 0x30);                          // WRITE SECTOR(S)
        
        for (int i = 0; i < sectors_to_write; i++) {
            while (!(inb(0x1F7) & 0x08));           // Wait for DRQ
            outsw(0x1F0, buffer, 256);        // Write 512 bytes (256 words)
            buffer += 512;
            lba++;
            count--;
        }
        
        // Flush the cache
        outb(0x1F7, 0xE7);                          // CACHE FLUSH
        while (inb(0x1F7) & 0x80);                  // Wait until not busy
    }
}

#endif