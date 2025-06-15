#pragma once

#include "../io.h"

typedef union {
    struct {
        u8 left   : 1;
        u8 right  : 1;
        u8 middle : 1;
        u8 unused : 5; // unused bits
    };
    
    u8 reg;
} MouseButtons;

typedef struct {
    i32 x;
    i32 y;
    
    MouseButtons buttons;
} MouseState;

extern MouseState mouse_state;

// Same data ports as the keyboard(PORT 1 of PS/2)
#define MOUSE_DATA_PORT    0x60
#define MOUSE_COMMAND_PORT 0x64

void mouse_init(void);

void mouse_wait_for_read(void);
u8 mouse_read(void);
void mouse_wait_for_write(void);
void mouse_write(u8 data);

// https://wiki.osdev.org/Text_Mode_Cursor
void enable_cursor(u8 cursor_start, u8 cursor_end);
void update_cursor(int x, int y);
u16 get_cursor_position(void);
void disable_cursor(void);

void mouse_irq(void);
