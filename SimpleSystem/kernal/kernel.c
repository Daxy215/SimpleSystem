#include "idt/idt.h"
#include "keyboard/keyboard.h"
#include "mouse/mouse.h"
#include "serial/serial.h"
#include "pic/pic.h"
#include "decoding/pictures/bmp.h" 

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
    
    // Identity map first 8MB (kernel space)
    pager_identity_map(pager, 0, 8*1024*1024, PAGE_PRESENT | PAGE_WRITE);
    
    // Enable paging
    pager_enable(pager);
}

// Kernel entry point
__attribute__((section(".text.start")))
void _start(void) {
    isr_install();
    
    // Remap PIC IRQs: master to 0x20, slave to 0x28
    PIC_remap(0x20, 0x28);
    
    // Enable paging
    setup_paging();
    
    irq_install_handler(2, handleIrq);
    irq_install_handler(12, mouse_irq);
    
    mouse_init();
    
    u32 stack_ptr;
    asm volatile("mov %%esp, %0" : "=r"(stack_ptr));
    printf("Initial ESP: %x\n", stack_ptr);
    
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

void print_stack_trace(const registers_t* regs) {
    u32* ebp = (u32*)regs->ebp;
    
    printf("\n--- Stack Trace (from EBP=");
    printf("%x", ebp);
    printf(") ---\n");
    
    u8 frame = 0;
    u8 max_frames = 16;
    
    // Add kernel stack boundary check
    u32 stack_start = 0x1000;
    u32 stack_end = 0xFFFFF000;
    
    printf("\n");
    
    for(int i = 0; i < 5; i++) {
        printf("Edp; %d\n", ebp[i]);
    }
    
    printf("\n");
    
    while (ebp && (frame < max_frames)) {
        // Check if EBP is within kernel stack
        if ((u32)ebp < stack_start || (u32)ebp > stack_end - 8) {
            printf("  Invalid EBP: 0x%x (frame %d)\n", ebp, frame);
            break;
        }
        
        // Check if return address is in kernel code
        u32 return_eip = ebp[1];
        if (return_eip < 0x100000 || return_eip > 0xFFFF0000) { // Adjust bounds
            printf("  Invalid EIP: %x (frame)\n", return_eip);
            break;
        }
        
        printf("Frame %d: EBP=%x  EIP=%x\n", frame, ebp, return_eip);
        
        ebp = (u32*)*ebp;  // Next frame
        frame++;
    }
    
    if (frame == 0)
        printf("  No valid stack frames found\n");
    
    printf("--- End of Stack Trace ---\n\n");
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

// 1 = draw white pixel
// 0 = transparent
static const u8 mouse_cursor[16][16] = {
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
}; 

static u32 pX, pY;

void draw_cursor(u32 x, u32 y) {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            put_pixel(i + pX, j + pY, 0, 0, 0);
        }
    }
    
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (mouse_cursor[i][j]) {
                put_pixel_rgba(x + j, y + i, 0x20FFFFFF);
            }
        }
    }
    
    pX = x;
    pY = y;
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
    u32 fb_addr = VESA_LFB_PTR;
    u16 fb_width = VESA_X_RES;
    u16 fb_height = VESA_Y_RES;
    u32 fb_bpp = VESA_BPP;
    
    //u32 fb_size = VESA_X_RES * VESA_Y_RES * (VESA_BPP/8);
    u32 fb_size = fb_width * fb_height * (fb_bpp / 8);
    
    // Map framebuffer
    pager_map_range(pager, fb_addr, fb_addr, fb_size, PAGE_PRESENT | PAGE_WRITE);
    
    init_graphics(fb_width, fb_height, fb_bpp, (u8*)fb_addr);
    pitch = VESA_X_RES * pixelwidth;
    
    printf("Graphics: %dx%dx%d @ %x PW=%d\n",
           fb_width, fb_height, fb_bpp, fb_addr, pixelwidth);
    
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
    
    u8 buffer[512];
    lba_read(partition_start, 1, buffer);
    
    FAT_BootSector* bs = (FAT_BootSector*)buffer;
    
    if (bs->signature != 0xAA55) {
        printf("Invalid boot sector signature: %x\n", bs->signature);
        
        return;
    }
    
    printf("Bytes per sector: %d\n", (unsigned int)bs->common.bytes_per_sector);
    printf("Sectors per cluster: %d\n", (unsigned int)bs->common.sectors_per_cluster);
    
    if (bs->common.root_entry_count == 0) {
        // Likely FAT32
        printf("FAT type: FAT32\n");
        
        printf("Root cluster: %x\n", bs->ebpb.f32.root_cluster);
    } else {
        // Likely FAT16
        printf("FAT type: FAT16\n");
        
        char vol_label[12]; // 11 chars + 1 null terminator
        u8* src = bs->ebpb.f16.volume_label;
        
        for (int i = 0; i < 11; i++) {
            vol_label[i] = src[i];
        }
        
        vol_label[11] = '\0';
        
        printf("Volume label: %s\n", vol_label);
        printf("Volume id: %d\n", bs->ebpb.f16.drive_number);
    }
    
    // ROOT_DIR_SECTORS = (MAX_ROOT_ENTRIES * 32 + BYTES_PER_SECTOR - 1) // BYTES_PER_SECTOR  # round up
    // FIRST_DATA_SECTOR = RESERVED_SECTORS + (NUM_FATS * FAT_SIZE_SECTORS) + ROOT_DIR_SECTORS
    
    //u32 first_fat_sector = bs->common.reserved_sector_count + (bs->common.fat_count * bs->common.fat_size_16) + root_dir_sectors;
    //unsigned int fat_sector = first_fat_sector + (fat_offset / sector_size);
    
    // FAT16 specific calculation:
    /*u32 root_dir_sectors = ((bs->common.root_entry_count * 32) + (bs->common.bytes_per_sector - 1)) / bs->common.bytes_per_sector;
    u32 first_data_sector = bs->common.reserved_sector_count +
                            (bs->common.fat_count * bs->common.fat_size_16) +
                            root_dir_sectors;*/
    
    u32 root_dir_start = bs->common.reserved_sector_count +
                      (bs->common.fat_count * bs->common.fat_size_16);
    
    u32 root_dir_sectors = ((bs->common.root_entry_count * 32) + (bs->common.bytes_per_sector - 1)) / bs->common.bytes_per_sector;
    u16 bytes_per_sector = bs->common.bytes_per_sector;
    u8 sectors_per_cluster = bs->common.sectors_per_cluster;
    
    u32 first_data_sector = bs->common.reserved_sector_count +
                            (bs->common.fat_count * bs->common.fat_size_16) +
                            root_dir_sectors;
    
    u32 total_sectors = bs->common.total_sectors_16 ?
                        bs->common.total_sectors_16 :
                        bs->common.total_sectors_32;
    
    #define sector_size bs->common.bytes_per_sector
    
    // Read data
    #define ROOT_DIR_SECTORS 32
    #define ROOT_DIR_BYTES (ROOT_DIR_SECTORS * sector_size)
    
    u8 RootDirBuffer[ROOT_DIR_BYTES];
    
    lba_read(root_dir_start, root_dir_sectors, RootDirBuffer);
    
    printf("Size: %x\n", ROOT_DIR_BYTES);
    
    // Max possible amount of files (For FAT16)
    DirEntry* entires[ROOT_DIR_BYTES / 32];
    u16 entriesCount = 0;
    
    for (int i = 0; i < ROOT_DIR_BYTES; i += 32) {
        DirEntry* entry = (DirEntry*)&RootDirBuffer[i];
        
        if (entry->name[0] == 0x00) {
            // a null file
            continue;
        }
        
        if ((u8)entry->name[0] == 0xE5) {
            printf("Skipp1\n");
            continue;
        }
        
        char name[12];
        u8* src = entry->name;
        
        for (int i = 0; i < 11; i++) {
            name[i] = src[i];
        }
        
        name[11] = '\0';
        
        printf("Name: %s Attr: %x Size: %x Cluster: %x\n",
               name, entry->attr, entry->size, entry->low_cluster);
        
        entires[entriesCount++] = entry;
    }
    
    #define END_OF_CLUSTER_MARKER 0xFFF8
    
    DirEntry* fileToRead = entires[1];
    
    char name[12];
    u8* src = fileToRead->name;
    
    for (int i = 0; i < 11; i++) {
        name[i] = src[i];
    }
    
    name[11] = '\0';
    
    printf("Reading %s\n", name);
    printf("File size: %x bytes\n", fileToRead->size);
    
    size_t sector_count = (fileToRead->size + 511) / 512;
    printf("Sectors to read from file: %x\n", sector_count);
    u8* fileBufer = memalign(4096, sector_count * 512);
    
    printf("Reading file...\n");
    
    printf("First data sector: 0x%x\n", first_data_sector);
    printf("Total sectors: 0x%x\n", total_sectors);
    
    // Start reading file
    u16 cluster = fileToRead->low_cluster;
    u32 offset = 0;
    
    printf("FAT details:\n");
    printf("  Reserved sectors: %x\n", bs->common.reserved_sector_count);
    printf("  FAT size: %x sectors\n", bs->common.fat_size_16);
    printf("  Bytes per sector: %x\n", bytes_per_sector);
    printf("  First data sector: %x\n", first_data_sector);
    printf("  Total sectors: %x\n", total_sectors);
    
    u32 totalNumOfDataSectors = total_sectors - (bs->common.reserved_sector_count + (bs->common.fat_count * bs->common.fat_size_16) + root_dir_sectors);
    u32 total_clusters = totalNumOfDataSectors / bs->common.sectors_per_cluster;
    
    if (bytes_per_sector == 0) {
        //fat_type = ExFAT;
        printf("EXFAT\n");
    } else if(total_clusters < 4085) {
        //fat_type = FAT12;
        printf("FAT12\n");
    } else if(total_clusters < 65525) {
        //fat_type = FAT16;
        printf("FAT16\n");
    } else {
        //fat_type = FAT32;
        printf("FAT32\n");
    }
    
    u32 bytes_per_cluster = bytes_per_sector * sectors_per_cluster;
    
    /**
     * If "table_value" is greater than or equal to (>=) 0xFFF8,
     * then there are no more clusters in the chain.
     * This means that the whole file has been read.
     * If "table_value" equals (==) 0xFFF7 then this,
     * cluster has been marked as "bad".
     * "Bad" clusters are prone to errors and should be avoided.
     * If "table_value" is not one of the above cases then,
     * it is the cluster number of the next cluster in the file.
     */
    while (cluster >= 0x0002 && cluster < 0xFFF0) {
        u32 sector = first_data_sector + (cluster - 2) * sectors_per_cluster;
        
        lba_read(partition_start + sector, sectors_per_cluster, fileBufer + offset);
        
        offset += bytes_per_cluster;
        
        // Read next FAT entry
        u32 fat_offset = cluster * 2;
        u32 fat_sector = bs->common.reserved_sector_count + (fat_offset / bytes_per_sector);
        u32 ent_offset = fat_offset % bytes_per_sector;
        
        u8 fat_buffer[bytes_per_sector];
        if (!lba_read(partition_start + fat_sector, 1, fat_buffer)) {
            printf("FAT read failed at sector %x\n", fat_sector);
            
            break;
        }
        
        // Check FAT signature
        /*if (fat_buffer[0] != bs->common.media_type) {
            printf("FAT signature mismatch: expected %x, got %x\n",
                   bs->common.media_type, fat_buffer[0]);
            
            break;
        }*/
        
        //u16 next_cluster = *(unsigned short*)&fat_buffer[ent_offset];
        u16 next_cluster = (u16)fat_buffer[ent_offset] | 
                           (u16)(fat_buffer[ent_offset + 1] << 8);
        
        // Try secondary FAT if entry is zero
        if (next_cluster == 0 && bs->common.fat_count > 1) {
            printf("Trying secondary FAT...\n");
            
            u32 secondary_fat = fat_sector + bs->common.fat_size_16;
            if (!lba_read(partition_start + secondary_fat, 1, fat_buffer)) {
                printf("Secondary FAT read failed\n");
                break;
            }
            
            next_cluster = (u16)fat_buffer[ent_offset] | 
                           (u16)(fat_buffer[ent_offset + 1] << 8);
            
            printf("Secondary FAT entry: %x -> %x\n", cluster, next_cluster);
        }
        
        cluster = next_cluster;
    }
    
    printf("File has been fully read!\n");
    
    if (fileBufer[0] != 'B' || fileBufer[1] != 'M') {
        printf("Not a valid BMP file! 0=%d 1=%d\n", fileBufer[0], fileBufer[1]);
        return;
    }
    
    // Main loop
    while(1) {
        handleIrqs();
        
        draw_cursor(mouse_state.x, mouse_state.y);
        //fillrect((u32)mouse_state.x, (u32)mouse_state.y, 255, 0, 0, 100, 100);
    }
}
