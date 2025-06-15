#include "idt.h"

#include "../Serial/serial.h"

extern void load_idt_asm(u32);

idt_entry_t idt[IDT_ENTRIES];
idt_ptr_t idt_ptr;

void set_idt_gate(int n, u32 handler) {
	idt[n].base_lo = handler & 0xFFFF;
	idt[n].sel     = 0x08;     // Kernel code segment
	idt[n].always0 = 0;
	idt[n].flags   = 0x8E;     // Present, ring 0, 32-bit interrupt gate
	idt[n].base_hi = (handler >> 16) & 0xFFFF;
}

void isr_install() {
	set_idt_gate(0,  (u32)isr0);
	set_idt_gate(1,  (u32)isr1);
	set_idt_gate(2,  (u32)isr2);
	set_idt_gate(3,  (u32)isr3);
	set_idt_gate(4,  (u32)isr4);
	set_idt_gate(5,  (u32)isr5);
	set_idt_gate(6,  (u32)isr6);
	set_idt_gate(7,  (u32)isr7);
	set_idt_gate(8,  (u32)isr8);
	set_idt_gate(9,  (u32)isr9);
	set_idt_gate(10, (u32)isr10);
	set_idt_gate(11, (u32)isr11);
	set_idt_gate(12, (u32)isr12);
	set_idt_gate(13, (u32)isr13);
	set_idt_gate(14, (u32)isr14);
	set_idt_gate(15, (u32)isr15);
	set_idt_gate(16, (u32)isr16);
	set_idt_gate(17, (u32)isr17);
	set_idt_gate(18, (u32)isr18);
	set_idt_gate(19, (u32)isr19);
	set_idt_gate(20, (u32)isr20);
	set_idt_gate(21, (u32)isr21);
	set_idt_gate(22, (u32)isr22);
	set_idt_gate(23, (u32)isr23);
	set_idt_gate(24, (u32)isr24);
	set_idt_gate(25, (u32)isr25);
	set_idt_gate(26, (u32)isr26);
	set_idt_gate(27, (u32)isr27);
	set_idt_gate(28, (u32)isr28);
	set_idt_gate(29, (u32)isr29);
	set_idt_gate(30, (u32)isr30);
	set_idt_gate(31, (u32)isr31);
	
	load_idt();
}

void load_idt() {
	idt_ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
	idt_ptr.base  = (u32)&idt;
	
	printf("Loading IDT at %x (limit=%x)\n", idt_ptr.base, idt_ptr.limit);
	load_idt_asm((u32)&idt_ptr);
}
