#pragma once

#include "../io.h"

#define IDT_ENTRIES 256

typedef struct {
	u16 base_lo;
	u16 sel;
	u8  always0;
	u8  flags;
	u16 base_hi;
} __attribute__((packed)) idt_entry_t;

typedef struct {
	u16 limit;
	u32 base;
} __attribute__((packed)) idt_ptr_t;

extern void isr0(); // Divide by zero
extern void isr1(); // Debug
extern void isr2(); // Non-maskable Interrupt
extern void isr3(); // Breakpoint
extern void isr4(); // Overflow
extern void isr5(); // Bound Range Exceeded
extern void isr6(); // Invalid Opcode
extern void isr7(); // Device Not Available
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

void set_idt_gate(int n, u32 handler);
void isr_install();
void load_idt();
