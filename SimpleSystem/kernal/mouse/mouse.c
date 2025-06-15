#include "mouse.h"
#include "../serial/serial.h"

MouseState mouse_state = {0};

void mouse_init() {
    mouse_state.x = 0;
    mouse_state.y = 0;
    
    // SET UP MOUSE (PS/2 PORT 2)
    
    mouse_wait_for_read();
    
    // Enable second PS/2 port (only if 2 PS/2 ports supported)
    outb(MOUSE_COMMAND_PORT, 0xA8);
    
    mouse_wait_for_read();
    
    // Get currently 
    outb(MOUSE_COMMAND_PORT, 0x20);
    u8 status = mouse_read();
    
    // Enable PS/2 port 2 interrupts
    status |= 2; // 0b10
    
    // Write back the configuration byte
    mouse_wait_for_read();
    outb(MOUSE_COMMAND_PORT, 0x60);
    mouse_wait_for_read();
    outb(MOUSE_DATA_PORT, status);
    
    // Use default settings
    // https://isdaman.com/alsos/hardware/mouse/ps2interface.htm
    mouse_write(0xF6);
    
    // Acknowledge
    if(mouse_read() != 0xFA) {
        printf("[ERROR] [MOUSE] FAILED");
    }
    
    mouse_write(0xF4);
    
    // Acknowledge
    if(mouse_read() != 0xFA) {
        printf("[ERROR] [MOUSE] FAILED");
    }
}

void waitForRead() {
    u32 timeout = TIMEOUT;
    
    while ((!(inb(MOUSE_COMMAND_PORT) & 0x01)) && (timeout--) > 0);
}

void mouse_wait_for_read() {
    u32 timeout = TIMEOUT;
    
    while ((inb(MOUSE_COMMAND_PORT) & 0x02) && (timeout--) > 0);
}

void mouse_write(u8 data) {
    /*
     * In order to write to the mouse,
     * the following must happen:
        * Write command '0xD4' to '0x64'
        * Setup some timer or counter to use as time-out
        * Poll bit 1 of Status Register ("Input buffer empty/full") until it becomes clear, or until your time-out expires.
        * If the time-out expired, return an error
        * Otherwise, write the data to the Data Port (IO port 0x60)
     */
    
    // TODO;
    /**
     * WARNING: If the PS/2 controller is an (older) single channel controller,
     * the command 0xD4 will be ignored, and therefore the byte will be sent to the first PS/2 device.
     * This means that (if you support older hardware) to reliably send data to the second device you,
     * have to know that the PS/2 Controller actually has a second PS/2 port.
     */
    
    // TODO; ERROR ON TIMEOUT
    mouse_wait_for_read();
    
    // Specify that this command,
    // must be sent to the Mouse
    outb(MOUSE_COMMAND_PORT, 0xD4);
    mouse_wait_for_read();
    outb(MOUSE_DATA_PORT, data);
}

u8 mouse_read() {
    waitForRead();
    return inb(MOUSE_DATA_PORT);
}

void enable_cursor(u8 cursor_start, u8 cursor_end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);

    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);   
}

void update_cursor(int x, int y) {
    u16 pos = y * /*VGA_WIDTH*/80 + x;
    
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8) (pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8) ((pos >> 8) & 0xFF));
}

u16 get_cursor_position(void) {
    u16 pos = 0;
    
    outb(0x3D4, 0x0F);
    pos |= inb(0x3D5);
    
    outb(0x3D4, 0x0E);
    pos |= ((u16)inb(0x3D5)) << 8;
    
    return pos;
}

void disable_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void mouse_irq(void) {
    static u8 data[3];
    static u8 byteIndex = 0;
    
    u8 status = inb(MOUSE_COMMAND_PORT);
    if (!(status & 0x01)) return;
    
    u8 byte = mouse_read();
    
    // Only process if byte 0 is valid (bit 3 must be set)
    if (byteIndex == 0 && (byte & 0x08) == 0) return;
    
    data[byteIndex++] = byte;
    
    if (byteIndex == 3) {
        byteIndex = 0;
        
        i8 dx = (i8)data[1];
        i8 dy = (i8)data[2];
        
        mouse_state.x += dx;
        mouse_state.y -= dy;
        
        mouse_state.buttons.reg = data[0] & 0x07;
        
        //printf("Mouse X: %d Y: %d\n", mouse_state.x, mouse_state.y);
    }
}
