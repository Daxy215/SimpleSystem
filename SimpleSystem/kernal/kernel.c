#include "idt/idt.h"
#include "keyboard/keyboard.h"
#include "mouse/mouse.h"
#include "serial/serial.h"
#include "pic/pic.h"
#include "decoding/pictures/bmp.h"
#include "memory/filesystem/filesystem.h"

#include "memory/paging.h"

// Explicit declaration before use
void kernel_main(void) __attribute__((section(".text.main")));
void _start(void) __attribute__((section(".text.start")));

// TODO; Video memory starts at 0xB8000

Pager* pager;

void setup_paging(void) {
    pager = pager_create();
    
    if (!pager) {
        printf("Pager couldn't be created!");
        
        while(1);
        
        return;
    }
    
    // 64MB (0x00000000 - 0x04000000)
    pager_identity_map(pager, 0, 64 * 1024 * 1024, PAGE_PRESENT | PAGE_WRITE);
    
    /*pager_identity_map(pager, 0x04000000, 0x2000000,  // 32MB
                      PAGE_PRESENT | PAGE_WRITE | PAGE_GLOBAL);*/
    
    // Enable paging
    pager_enable(pager);
}

// Kernel entry point
__attribute__((section(".text.start")))
void _start(void) {
    u32 stack_ptr;
    asm volatile("mov %%esp, %0" : "=r"(stack_ptr));
    printf("Initial ESP: %x\n", stack_ptr);
    
    isr_install();
    
    // Remap PIC IRQs: master to 0x20, slave to 0x28
    PIC_remap(0x20, 0x28);
    
    mouse_init();
    
    //irq_install_handler(0, timer_stub);
    irq_install_handler(2, handleIrq);
    irq_install_handler(12, mouse_irq);
    
    // Enable paging
    setup_paging();
    
    kernel_main();
    
    // Infinite loop to prevent exit
    while(1);
}

typedef struct {
    u32 eax, ecx, edx, ebx;
    u32 esp;
    u32 ebp, esi, edi;
    u32 ds, es, fs, gs;
    u32 int_no, err_code;
    u32 eip, cs, eflags;
} registers_t;

void print_stack_trace(uintptr_t ebp) {
    printf(">>> Starting stack trace from EBP: %x\n", ebp);
    
    int frame = 0;
    
    while (frame < 32 && ebp/* && is_valid_addr(ebp) && is_valid_addr(ebp + 4)*/) {
        uintptr_t *frame_ptr = (uintptr_t *)ebp;
        uintptr_t next_ebp = frame_ptr[0];
        uintptr_t ret_addr = frame_ptr[1];
        
        /*if (!is_valid_addr(ret_addr)) {
            printf("  Invalid EIP: %x (frame %d)\n", ret_addr, frame);
            break;
        }*/
        
        printf("  Frame %x: EBP=%x, RET=%x\n", frame, ebp, ret_addr);
        
        if (next_ebp == 0 || next_ebp <= ebp) break; // prevent infinite loop

        ebp = next_ebp;
        frame++;
    }
    
    printf("Stack ended");
}

void isr_handler_c(registers_t* regs) {
    const char* exception_messages[] = {
        "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
        "Into Detected Overflow", "Out of Bounds", "Invalid Opcode", "No Coprocessor",
        "Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
        "Stack Fault", "General Protection Fault", "Page Fault", "Unknown Interrupt",
        "Coprocessor Fault", "Alignment Check", "Machine Check", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved"
    };
    
    printf("\n********** EXCEPTION OCCURRED **********\n");
    
    printf("Interrupt Number: %d (", regs->int_no);
    if (regs->int_no < 32)
        printSerial(exception_messages[regs->int_no]);
    else
        printSerial("Unknown");
    
    printSerial(")\n");
    
    if (regs->int_no == 14) { // Page Fault
        u32 fault_addr;
        asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
        
        printf("\nPAGE FAULT @ %x\n", fault_addr);
        printf("Error Code: %x\n", regs->err_code);
        
        // Decode error code
        const char* rw    = (regs->err_code & 0x2) ? "Write"      : "Read";
        const char* priv  = (regs->err_code & 0x4) ? "User"       : "Kernel";
        const char* cause = (regs->err_code & 0x1) ? "Protection" : "Not Present";
        
        printf("%s access at %x (%s mode, %s)\n", rw, fault_addr, priv, cause);
    }
    
    print_stack_trace(regs);
    
    // Print register dump
    printf("\nRegister Dump:\n");
    printf("EAX=%x EBX=%x ECX=%x EDX=%x\n", regs->eax, regs->ebx, regs->ecx, regs->edx);
    printf("ESI=%x EDI=%x EBP=%x ESP=%x\n", regs->esi, regs->edi, regs->ebp, regs->esp);
    printf("EIP=%x EFLAGS=%x\n", regs->eip, regs->eflags);
    
    printf("********** SYSTEM HALTED **********\n");
    
    while(1);
}

// TODO; Move this..
#define VESA_INFO_ADDR  0x00007E00

#define VESA_X_RES      (*(u16*)(VESA_INFO_ADDR + 0x00))
#define VESA_Y_RES      (*(u16*)(VESA_INFO_ADDR + 0x02))
#define VESA_BPP        (*(u8 *)(VESA_INFO_ADDR + 0x04))
#define VESA_LFB_PTR    (*(u32*)(VESA_INFO_ADDR + 0x06))

int pixelwidth;
int pitch;
u8 *framebuffer_base;

void init_graphics(int width, int height, int bpp, u8* fb_base) {
    pixelwidth       = bpp / 8;
    pitch            = width * pixelwidth;
    framebuffer_base = fb_base;
}

// Blend two color channels with alpha
u8 blend_channel(u8 src, u8 dst, u8 alpha) {
    return (src * alpha + dst * (255 - alpha)) / 255;
}

void put_pixel(u32 x, u32 y, u8 r, u8 g, u8 b) {
    if (x >= VESA_X_RES || y >= VESA_Y_RES) return;
    
    u8* pixel = framebuffer_base + (y * VESA_X_RES + x) * pixelwidth;
    
    //u8 r = (src_r * alpha + dst_r * (255 - alpha)) / 255;
    //u8 g = (src_g * alpha + dst_g * (255 - alpha)) / 255;
    //u8 b = (src_b * alpha + dst_b * (255 - alpha)) / 255;
    
    pixel[0] = b;
    pixel[1] = g;
    pixel[2] = r;
}

// Pixel format (0xAARRGGBB)
void put_pixel_rgba(int x, int y, u32 rgba) {
    if (x >= VESA_X_RES || y >= VESA_Y_RES) return;
    
    u8 src_r = (rgba >> 16) & 0xFF;
    u8 src_g = (rgba >> 8) & 0xFF;
    u8 src_b = (rgba) & 0xFF;
    u8 alpha = (rgba >> 24) & 0xFF;
    
    u8* dst = framebuffer_base + y * pitch + x * 3;
    
    u8 dst_r = dst[0];
    u8 dst_g = dst[1];
    u8 dst_b = dst[2];
    
    dst[0] = blend_channel(src_r, dst_r, alpha); // Red
    dst[1] = blend_channel(src_g, dst_g, alpha); // Green
    dst[2] = blend_channel(src_b, dst_b, alpha); // Blue
}

static void fillrect(u32 x, u32 y, u8 r, u8 g, u8 b, u32 w, u32 h) {
    if(x < 0 || x >= VESA_X_RES
        || y < 0 || y >= VESA_Y_RES)
        return;
    
    for (u32 i = 0; i < w; i++) {
        for (u32 j = 0; j < h; j++) {
            put_pixel(x + i, y + j, r, g, b);
        }
    }
}

// TODO; Move else where
i32 floor_div(i32 a, i32 b) {
    i32 q = a / b;
    i32 r = a % b;
    
    // If remainder != 0 and signs differ, subtract 1 to floor toward -∞
    if ((r != 0) && ((a < 0) != (b < 0))) {
        q -= 1;
    }
    
    return q;
}

void kernel_main(void) {
    //return;
    u32 fb_addr = VESA_LFB_PTR;
    u16 fb_width = VESA_X_RES;
    u16 fb_height = VESA_Y_RES;
    u32 fb_bpp = VESA_BPP;
    
    //u32 fb_size = VESA_X_RES * VESA_Y_RES * (VESA_BPP/8);
    u32 fb_size = fb_width * fb_height * (fb_bpp / 8);
    
    // Map framebuffer
    printf("FB_ADDR: %x\n", fb_addr);
    u32* vesa_info = (u32*)0x7E00;
    printf("Framebuffer at %x\n", vesa_info[6]);
    
    pager_map_range(pager, fb_addr, fb_addr, fb_size, PAGE_PRESENT | PAGE_WRITE);
    
    init_graphics(fb_width, fb_height, fb_bpp, (u8*)fb_addr);
    pitch = VESA_X_RES * pixelwidth;
    
    printf("Graphics: %dx%dx%d @ %x PW=%d\n",
           fb_width, fb_height, fb_bpp, fb_addr, pixelwidth);
    
    fillrect(100, 100, 255, 0, 0, 200, 200);
    
    FATSystem* system = fs_createSystem(34);
    fs_open(system, &system->entries[1]);
    
    // Draws a .bmp test image
    /*{
        u8 temp[512];
        lba_read(150, 1, temp);
        
        BITMAPFILEHEADER* fileHeader = (BITMAPFILEHEADER*)temp;
        u32 full_size = fileHeader->bfSize;
        u32 sector_count = (full_size + 511) / 512;
        
        printf("Reading %d sectors\n", sector_count);
    
        u8* buffer = memalign(4096, sector_count * 512);
    
        if (!buffer) {
            printf("Allocation failed\n");
            return;
        }
    
        memset(buffer, 0, sector_count * 512);
        lba_read(150, sector_count, buffer);
    
        if (buffer[0] != 'B' || buffer[1] != 'M') {
            printf("Not a valid BMP file! 0=%d 1=%d\n", buffer[55], buffer[1]);
        
            return;
        }
        
        fileHeader = (BITMAPFILEHEADER*)buffer;
        BITMAPINFOHEADER* infoHeader = (BITMAPINFOHEADER*)(buffer + sizeof(BITMAPFILEHEADER));
    
        printf("BITMAPFILEHEADER: %d bytes\n", sizeof(BITMAPFILEHEADER));
        printf("BITMAPINFOHEADER: %d bytes\n", sizeof(BITMAPINFOHEADER));
    
        if (infoHeader->biWidth <= 0 || infoHeader->biHeight == 0) {
            printf("Invalid BMP dimensions!\n");
            return;
        }
    
        u32 pixel_offset = fileHeader->bfOffBits;
        if (pixel_offset >= sector_count * 512) {
            printf("Invalid pixel data offset! = %x\n", pixel_offset);
            return;
        }
    
        int width = infoHeader->biWidth;
        int height = infoHeader->biHeight;
        int rowSize = ((width * 3 + 3) / 4) * 4;
        u8* pixelData = buffer + fileHeader->bfOffBits;
    
        printf("Width=%d, Height=%d, BitCount=%d\n", 
           width, height, infoHeader->biBitCount);

        // Verify this is a 24-bit BMP
        if (infoHeader->biBitCount != 24) {
            printf("Only 24-bit BMPs supported!\n");
            return;
        }
    
        int screen_width = VESA_X_RES;
        int screen_height = VESA_Y_RES;
    
        int draw_width = width < screen_width ? width : screen_width;
        int draw_height = height < screen_height ? height : screen_height;
    
        u8* row;
    
        for (int y = 0; y < draw_height; y++) {
            row = pixelData + (height - 1 - y) * rowSize;
        
            for (int x = 0; x < draw_width; x++) {
                int px = x * 3;
            
                u8 b = row[px];
                u8 g = row[px + 1];
                u8 r = row[px + 2];
            
                put_pixel(x, y, r, g, b);
            }
        }
    }
    */
    
    //if (fileBufer[0] != 'B' || fileBufer[1] != 'M') {
    //    printf("Not a valid BMP file! 0=%d 1=%d\n", fileBufer[0], fileBufer[1]);
    //    return;
    //}
    
    // Main loop
    while(1) {
        handleIrqs();
        
        //draw_cursor(mouse_state.x, mouse_state.y);
        fillrect((u32)mouse_state.x, (u32)mouse_state.y, 255, 0, 0, 100, 100);
    }
}
