#pragma once

#include "../io.h"

// The 2 32-bit I/O locations

/**
 *  CONFIG_ADDRESS specifies the configuration address that is required to be accesses,
 *
 *  BIT  31 -    Enable bit
 *  BITs 30 - 24 Reserved
 *  BITs 23 - 16 Bus Number
 *  BITs 15 - 11 Device Number
 *  Bits 10 - 8  Function Number
 *  bITS 8  - 0  Register Offset
 */
#define CONFIG_ADDRESS 0xCF8

/**
 * while accesses to CONFIG_DATA will actually generate the configuration access and will transfer the data to or from the CONFIG_DATA register.
 * 
 */
#define CONFIG_DATA    0xCFC

u16 pciConfigReadWord(u8 bus, u8 slot, u8 func, u8 offset) {
	u32 lbus  = (u32)bus;
	u32 lslot = (u32)slot;
	u32 lfunc = (u32)func;
	
	// Create configuration address as per Figure 1
	u32 address = (u32)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) | ((u32)0x80000000));
	
	// Write out the address
	outl(CONFIG_ADDRESS, address);
	
	// Read in the data
	// (offset & 2) * 8) = 0 will choose the first word of the 32-bit register
	u16 tmp = (u16)((inl(CONFIG_DATA) >> ((offset & 2) * 8)) & 0xFFFF);
	
	return tmp;
}

u32 pciConfigReadDword(u8 bus, u8 slot, u8 func, u8 offset) {
	u32 address = (u32)((bus << 16) | (slot << 11) |
						(func << 8) | (offset & 0xFC) | 0x80000000);
	
	outl(CONFIG_ADDRESS, address);
	
	return inl(CONFIG_DATA);
}

// TODO; REMOVE
u32 getHeaderInformation(u8 type) {
	switch (type & 0x7F) { // bit 7 = multifunction flag
		case 0x00: {
			printf("  -> Header Type 0x0 (General Device)\n");
			printf("     BAR0: %x\n", pciConfigReadDword(0, 0, 0, 0x10));
			printf("     BAR1: %x\n", pciConfigReadDword(0, 0, 0, 0x14));
			printf("     BAR2: %x\n", pciConfigReadDword(0, 0, 0, 0x18));
			printf("     BAR3: %x\n", pciConfigReadDword(0, 0, 0, 0x1C));
			printf("     BAR4: %x\n", pciConfigReadDword(0, 0, 0, 0x20));
			printf("     BAR5: %x\n", pciConfigReadDword(0, 0, 0, 0x24));
			
			u32 romAddr = pciConfigReadDword(0, 0, 0, 0x30);
			u32 irqReg  = pciConfigReadDword(0, 0, 0, 0x3C);
			u8  irqLine = irqReg & 0xFF;
			u8  irqPin  = (irqReg >> 8) & 0xFF;
			
			printf("     Expansion ROM Base: %x\n", romAddr);
			printf("     Interrupt Line: %d, Pin: %d\n", irqLine, irqPin);
			
			break;
		}
		
		case 0x01: {
			printf("  -> Header Type 0x1 (PCI-to-PCI Bridge)\n");
			
			u32 busReg = pciConfigReadDword(0, 0, 0, 0x18);
			u8 primaryBus     = busReg & 0xFF;
			u8 secondaryBus   = (busReg >> 8) & 0xFF;
			u8 subordinateBus = (busReg >> 16) & 0xFF;
			u8 latencyTimer   = (busReg >> 24) & 0xFF;
			
			printf("     Primary Bus: %d, Secondary Bus: %d, Subordinate Bus: %d\n",
					primaryBus, secondaryBus, subordinateBus);
			
			break;
		}
		
		case 0x02: {
			printf("  -> Header Type 0x2 (PCI-to-CardBus Bridge)\n");
			
			u32 baseAddr = pciConfigReadDword(0, 0, 0, 0x10);
			
			printf("     CardBus Base Address: %x\n", baseAddr);
			
			break;
		}
		
		default:
			printf("  -> Unknown Header Type: 0x%x\n", type);
		break;
	}
	
	return 0;
}

#define FRAME_LIST_COUNT 1024

typedef union PACKED {
	u32 value;
	
	struct {
		u32 frame_enable : 1;   // Bit 0: 0 = valid, 1 = empty
		u32 type : 1;           // Bit 1: 0 = TD, 1 = QH
		u32 reserved : 2;       // Bits 3:2
		u32 phys_addr : 28;     // Bits 31:4 - physical address to TD/QH (>> 4)
	};
} uhci_frame_list_entry_t;

typedef struct ALIGNED(16) {
	// 0x00 - Horizontal Pointer (next QH or TD)
	union {
		u32 horizontal_ptr;
		
		// Same as Frame List Entry
		struct {
			u32 horiz_terminate : 1;  // Bit 0 - 1 = no more
			u32 horiz_type : 1;       // Bit 1 - 0=TD, 1=QH
			u32 horiz_reserved : 2;   // Bits 3:2
			u32 horiz_phys_addr : 28; // Bits 31:4 - physical address >> 4
		};
	};
	
	// 0x04 - Vertical Pointer (first TD or QH to execute)
	union {
		u32 vertical_ptr;
		
		// Same as Frame List Entry
		struct {
			u32 vert_terminate : 1;   // Bit 0
			u32 vert_type : 1;        // Bit 1
			u32 vert_reserved : 2;    // Bits 3:2
			u32 vert_phys_addr : 28;  // Bits 31:4
		};
	};
	
	// 0x08 - Software use / padding
	u32 reserved[2]; // to keep it 16-byte alignment
} uhci_qh_t;

typedef struct ALIGNED(16) {
    // 0x00 - Next Descriptor Pointer
    union {
        u32 next;
        struct {
            u32 terminate : 1;      // Bit 0
            u32 type : 1;           // Bit 1 (0 = TD, 1 = QH)
            u32 depth : 1;          // Bit 2
            u32 reserved0 : 1;      // Bit 3
            u32 link_ptr : 28;      // Bits 31:4 - Physical address of next TD/QH
        };
    };
	
    // 0x04 - Status
    union {
        u32 status;
    	
        struct {
            u32 actual_length : 11;     // Bits 10:0 (length - 1)
            u32 reserved1 : 5;          // Bits 15:11
            u32 bitstuff_err : 1;       // Bit 17
            u32 crc_timeout_err : 1;    // Bit 18
            u32 nak_received : 1;       // Bit 19
            u32 babble_detected : 1;    // Bit 20
            u32 buffer_error : 1;       // Bit 21
            u32 stalled : 1;            // Bit 22
            u32 active : 1;             // Bit 23
            u32 int_on_complete : 1;    // Bit 24
            u32 isochronous : 1;        // Bit 25
            u32 low_speed : 1;          // Bit 26
            u32 error_counter : 2;      // Bits 28–27
            u32 short_packet_detect : 1;// Bit 29
            u32 reserved2 : 2;          // Bits 31–30
        };
    };
	
    // 0x08 - Packet Header
    union {
        u32 packet_header;
    	
        struct {
            u32 packet_type : 8;    // Bits 7:0 (0x69=IN, 0xE1=OUT, 0x2D=SETUP)
            u32 device_addr : 7;    // Bits 14:8
            u32 endpoint : 4;       // Bits 18:15
            u32 data_toggle : 1;    // Bit 19
            u32 reserved3 : 1;      // Bit 20
            u32 max_len : 11;       // Bits 31:21 (Length - 1)
        };
    };
	
    // 0x0C - Buffer Address (Physical)
    u32 buffer_ptr;
	
    // 0x10 - System use / software-reserved area (16 bytes)
    u32 system_use[4];
} uhci_td_t;

// TODO; REMOVE

u16 pciCheckVendor(u8 bus, u8 slot, u8 func) {
	u16 vendor, device;
	
    /* vendors that == 0xFFFF, it must be a non-existent device. */
	if ((vendor = pciConfigReadWord(bus, slot, func, 0)) != 0xFFFF) {
   		device = pciConfigReadWord(bus, slot, func, 0x02);
		
		/**
		 * At 0x04 offset;
		 * BITS 31-24 - Status
		 * BITS 15-0  - Command
		 */
		u32 statusReg = pciConfigReadDword(bus, slot, func, 0x04);
		u16 status  = (statusReg >> 16) & 0xFFFF;
		u16 command = statusReg & 0xFFFF;

		/**
		 * At 0x08 offset;
		 * BITS 31-24 - Class Code
		 * BITS 23-16 - Subclass
		 * BITS 15-9  - Prog IF (Programming Interface Byte)
		 * BITS 7-0   - Revision ID
		 */
		u32 classReg  = pciConfigReadDword(bus, slot, func, 0x08);
		u8 classCode  = (classReg >> 24) & 0xFF;
		u8 subclass   = (classReg >> 16) & 0xFF;
		u8 progIf     = (classReg >> 8) & 0xFF;
		u8 revisionId = classReg & 0xFF;
		
		/**
		 * At offset 0xC;
		 * BITS 31-24 - BIST
		 * BITS 23-16 - Header Type
		 * BITS 15-9  - Latency Timer
		 * BITS 7-0   - Cache Line Size
		 */
		u32 bistReg      = pciConfigReadDword(bus, slot, func, 0x0C);
		u8 bist          = (bistReg >> 24) & 0xFF;
		
		/**
		 * 0x0 -> a general device
		 * 0x1 -> a PCI-to-PCI bridge
		 * 0x2 -> a PCI-to-CardBus bridge
		 */
		u8 headerType    = (bistReg >> 16) & 0xFF;
		u8 latencyTimer  = (bistReg >> 8) & 0xFF;
		u8 cacheLineSize = bistReg & 0xFF;
		
		//printf("PCI: bus %d, slot %d, func %d -> vendor=%x device=%x class=%x subclass=%x progif=%x headerType = %x\n",
		//		   bus, slot, func, vendor, device, classCode, subclass, progIf, headerType);
		
		// TODO; All of this needs to be moved!
		if (classCode == 0x0C && subclass == 0x03) {
			printf("  -> USB controller detected! (prog-if %x)\n", progIf);
			
			if(progIf == 0x00) {
				// If progIF is 0x00 handle -> UHCI Controller ()

				u32 bar0 = pciConfigReadDword(bus, slot, func, 0x10);
				u32 bar1 = pciConfigReadDword(bus, slot, func, 0x14);
				u32 bar2 = pciConfigReadDword(bus, slot, func, 0x18);
				u32 bar3 = pciConfigReadDword(bus, slot, func, 0x1C);
				u32 bar4 = pciConfigReadDword(bus, slot, func, 0x20);
				u32 bar5 = pciConfigReadDword(bus, slot, func, 0x24);
				
				printf("  -> Header Type 0x0 (General Device)\n");
				printf("     BAR0: %x\n", bar0);
				printf("     BAR1: %x\n", bar1);
				printf("     BAR2: %x\n", bar2);
				printf("     BAR3: %x\n", bar3);
				printf("     BAR4: %x\n", bar4);
				printf("     BAR5: %x\n", bar5);
				
				u32 romAddr = pciConfigReadDword(bus, slot, func, 0x30);
				u32 irqReg  = pciConfigReadDword(bus, slot, func, 0x3C);
				// BITS 31-24 - Max Latency
				// BITS 23-16 - Min Grant
				u8  irqPin  = (irqReg >> 8) & 0xFF;
				u8  irqLine = irqReg & 0xFF;
				
				printf("     Expansion ROM Base: %x\n", romAddr);
				printf("     Interrupt Line: %d, Pin: %d\n", irqLine, irqPin);
				
				// UHCI I/O port address information is in BAR4
				/**
				 * Offset (Hex) 	Name 	      Description 	           Lenght
				 * 00 	            USBCMD 	      Usb Command 	            2 bytes
				 * 02 	            USBSTS 	      Usb Status 	            2 bytes
				 * 04 	            USBINTR       Usb Interrupt Enable 	    2 bytes
				 * 06 	            FRNUM 	      Frame Number       	    2 bytes
				 * 08 	            FRBASEADD     Frame List Base Address   4 bytes
				 * 0C 	            SOFMOD 	      Start Of Frame Modify 	1 byte
				 * 10 	            PORTSC1       Port 1 Status/Control 	2 bytes
				 * 12 	            PORTSC2       Port 2 Status/Control 	2 bytes
				 */
				// TODO; readUHCIRegisters
				
				/**
				 * Base Address Registors layout;
				 *	Memory Space BR Layout;
				 *		BITS 31-4 - 16-Byte Aligned Base Addres
				 *		BIT  3    - Prefetchable
				 *		BITS 2-1  - Type
				 *		BIT  0    - Always 0
				 *	
				 *	I/O SPace BAR Layout;
				 *		BITS 31-2 - 4-Byte Aligned Base Address
				 *		BIT  1    - Reserved
				 *		BIT  0    - Always 1
				 * 
				 * To distinguish between them, you can check the value of the lowest bit.
				 */
				
				// Check for bit 0
				u8 barType = (bar4 & 0x1);
				
				printf("Bar type; %x\n", barType);
				
				// This is hardcoded BUT, for this,
				// barType is 1, so it's an I/O Space BAR Layout,
				// TODO; Handle when barType is 0 just use (~0x02) instead 
				u16 ioBase = (u16)(bar4 & ~0x03); // Mask lower bits
				
				if (!ioBase) {
					printf("  -> UHCI: BAR4 invalid (%x)\n", bar4);
					
					return 0;
				}
				
				printf("  -> UHCI I/O Base Address: %x\n", ioBase);
				
				// Read UHCI registers
				u16 USBCMD     = inw(ioBase + 0x00);
				u16 USBSTS     = inw(ioBase + 0x02);
				u16 USBINTR    = inw(ioBase + 0x04);
				u16 FRNUM      = inw(ioBase + 0x06);
				u32 FRBASEADD  = inl(ioBase + 0x08);
				u8  SOFMOD     = inb(ioBase + 0x0C);
				u16 PORTSC1    = inw(ioBase + 0x10);
				u16 PORTSC2    = inw(ioBase + 0x12);
				
				printf("  -> UHCI Register Dump:\n");
				printf("     USBCMD     = %x\n", USBCMD);
				printf("     USBSTS     = %x\n", USBSTS);
				printf("     USBINTR    = %x\n", USBINTR);
				printf("     FRNUM      = %x\n", FRNUM);
				printf("     FRBASEADD  = %x\n", FRBASEADD);
				printf("     SOFMOD     = %x\n", SOFMOD);
				printf("     PORTSC1    = %x\n", PORTSC1);
				printf("     PORTSC2    = %x\n", PORTSC2);
				
				/**
				 * 32-bit physical adress of Frame List.
				 * Address has to be aligned to 4 Kb (first 12 bits are zero).
				 * The Frame List must contain 1024 entries.
				 */
				uhci_frame_list_entry_t* frame_list = memalign(4096, sizeof(uhci_frame_list_entry_t) * FRAME_LIST_COUNT);
				memset(frame_list, 0, sizeof(uhci_frame_list_entry_t) * FRAME_LIST_COUNT);
				
				// QH
				uhci_qh_t* qh = memalign( 16, sizeof(uhci_qh_t));
				memset(qh, 0, sizeof(uhci_qh_t));
				
				uintptr_t qh_phys = (uintptr_t)qh; // TODO; convert to physical
				frame_list[0].phys_addr    = (qh_phys >> 4);
				frame_list[0].type         = 1; // QH
				frame_list[0].frame_enable = 0;
				
				qh->horizontal_ptr = 1; // Terminate (bit 0 = 1 means end)
				qh->vertical_ptr  = 1;  // No TDs linked yet
				
				outl((uintptr_t)frame_list & 0xFFFFF000, ioBase + 0x08);

				// Begin processing
				u16 cmd = inw(ioBase + 0x00);
				cmd |= 0x0001; // Run/Stop = 1
				outw(cmd, ioBase + 0x00);
				
				
			}
			
			// If progIF is 0x10 handle -> OHCI Controller ()
			// If progIF is 0x20 handle -> EHCI (USB2) Controller ()
			// If progIF is 0x30 handle -> XHCI (USB3) Controller (https://wiki.osdev.org/EXtensible_Host_Controller_Interface)
			// If progIF is 0xFE handle -> USB Device (Not a host controller) 
		}
	} return (vendor);
}

// Maybe do this instead?

/*void checkDevice(uint8_t bus, uint8_t device) {
	uint8_t function = 0;

	vendorID = getVendorID(bus, device, function);
	if (vendorID == 0xFFFF) return; // Device doesn't exist
	checkFunction(bus, device, function);
	headerType = getHeaderType(bus, device, function);
	if( (headerType & 0x80) != 0) {
		// It's a multi-function device, so check remaining functions
		for (function = 1; function < 8; function++) {
			if (getVendorID(bus, device, function) != 0xFFFF) {
				checkFunction(bus, device, function);
			}
		}
	}
}*/