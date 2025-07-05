#include "pic.h"

#include "../keyboard/keyboard.h"
#include "../serial/serial.h"

void irq_install_handler(u8 irq, irq_handler_t handler) {
	irq_routines[irq] = handler;
}

void irq_uninstall_handler(u8 irq) {
	irq_routines[irq] = 0;
}

void PIC_sendEOI(u8 irq) {
	if(irq >= 8)
		outb(PIC2_COMMAND,PIC_EOI);
	
	outb(PIC1_COMMAND,PIC_EOI);
}

void PIC_remap(int offset1, int offset2) {
	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  // starts the initialization sequence (in cascade mode)
	io_wait();
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();
	outb(PIC1_DATA, offset1);                 // ICW2: Master PIC vector offset
	io_wait();
	outb(PIC2_DATA, offset2);                 // ICW2: Slave PIC vector offset
	io_wait();
	outb(PIC1_DATA, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	io_wait();
	outb(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
	io_wait();
	
	outb(PIC1_DATA, ICW4_8086);               // ICW4: have the PICs use 8086 mode (and not 8080 mode)
	io_wait();
	outb(PIC2_DATA, ICW4_8086);
	io_wait();
	
	// Unmask both PICs.
	outb(PIC1_DATA, 0);
	outb(PIC2_DATA, 0);
	
	for(int i = 0; i < 16; i++)
		IRQ_clear_mask(i);
}

void pic_disable(void) {
	outb(PIC1_DATA, 0xff);
	outb(PIC2_DATA, 0xff);
}

void handleIrqs() {
	u16 irr = pic_get_irr();
	
    /**
     * IRQ 0  – Timer: A hardware timer interrupt (typically used by the system clock).
	 * IRQ 1  – Keyboard: Triggered by keyboard input.
	 * IRQ 2  – Cascade (PIC2): Used for communication between the two PICs in a cascade setup.
	 * IRQ 3  – COM2 (Serial port 2): For serial communication (often used for RS-232 devices).
	 * IRQ 4  – COM1 (Serial port 1): For serial communication (often used for RS-232 devices).
	 * IRQ 5  – LPT2 (Parallel port 2): For parallel communication (often used for printers).
	 * IRQ 6  – Floppy Disk Controller (FDC): Handles interrupt requests from floppy disk drives.
	 * IRQ 7  – LPT1 (Parallel port 1): For parallel communication (often used for printers).
	 * IRQ 8  – Real-Time Clock (RTC): Provides timekeeping and periodic interrupts from the system clock.
	 * IRQ 9  – Reserved/IRQ Vector 9: Often used for other devices or system purposes.
	 * IRQ 10 – Reserved/IRQ Vector 10: Often used for other devices or system purposes.
	 * IRQ 11 – Reserved/IRQ Vector 11: Often used for other devices or system purposes.
	 * IRQ 12 – PS/2 Mouse: Used for PS/2 mouse input.
	 * IRQ 13 – Math Coprocessor (FPU): For handling floating-point operations.
	 * IRQ 14 – Primary IDE (Hard Disk): For interrupt requests from the primary hard disk controller (IDE).
	 * IRQ 15 – Secondary IDE (Hard Disk): For interrupt requests from the secondary hard disk controller (IDE).
     */
    // Loop through all possible IRQs (0-15)
    for (int irq = 0; irq < NUM_IRQS; irq++) {
    	// Whether the IRQ should be triggered or not
    	if (irr & (1 << irq)) {
    		//printf("IRQ; %d\n", irq);
    		
    		if(irq_routines[irq]) {
    			irq_routines[irq]();
    		} else {
    			//printf("Unknown IRQ: %d\n", irq);
    		}
    		
    		// Acknowledge the interrupt
    		PIC_sendEOI(irq);
    	}
    }
}

void IRQ_set_mask(u8 IRQline) {
	u16 port;
	u8 value;
	
	if(IRQline < 8) {
		port = PIC1_DATA;
	} else {
		port = PIC2_DATA;
		IRQline -= 8;
	}
	
	value = inb(port) | (1 << IRQline);
	outb(port, value);
}

void IRQ_clear_mask(u8 IRQline) {
	u16 port;
	u8 value;
	
	if(IRQline < 8) {
		port = PIC1_DATA;
	} else {
		port = PIC2_DATA;
		IRQline -= 8;
	}
	
	value = inb(port) & ~(1 << IRQline);
	outb(port, value); 
}

u16 __pic_get_irq_reg(int ocw3) {
	/* OCW3 to PIC CMD to get the register values.  PIC2 is chained, and
	 * represents IRQs 8-15.  PIC1 is IRQs 0-7, with 2 being the chain */
	
	outb(PIC1_COMMAND, ocw3);
	outb(PIC2_COMMAND, ocw3);
	return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

/*
 * pic_get_irr - Get the Interrupt Request Register (IRR) value.
 * 
 * The Interrupt Request Register (IRR) contains a bitmap of all IRQs 
 * that are currently pending and have not yet been acknowledged by the 
 * CPU. If a bit is set, it indicates that the corresponding IRQ is 
 * waiting to be serviced. This function retrieves the IRR value from 
 * both PIC1 and PIC2.
 *
 * TLDR; Retrieves the status of pending IRQs (those that need service).
 * 
 * Returns:
 *   A 16-bit value representing the IRQ request status. 
 *   Each bit corresponds to an IRQ line (IRQ 0-7 for PIC1, IRQ 8-15 for PIC2).
 *   A bit set to '1' means that the IRQ is pending.
 */
u16 pic_get_irr(void) {
	return __pic_get_irq_reg(PIC_READ_IRR);
}

/*
 * pic_get_isr - Get the Interrupt Service Register (ISR) value.
 * 
 * The Interrupt Service Register (ISR) contains a bitmap of IRQs 
 * that are currently being serviced. If a bit is set, it means the 
 * corresponding IRQ is being processed by the CPU. This function 
 * retrieves the ISR value from both PIC1 and PIC2.
 * 
 * TLDR; Retrieves the status of IRQs that are being serviced (has already been handled).
 * 
 * Returns:
 *   A 16-bit value representing the IRQ service status.
 *   Each bit corresponds to an IRQ line (IRQ 0-7 for PIC1, IRQ 8-15 for PIC2).
 *   A bit set to '1' means that the IRQ is currently being serviced.
 */
u16 pic_get_isr(void) {
	return __pic_get_irq_reg(PIC_READ_ISR);
}
