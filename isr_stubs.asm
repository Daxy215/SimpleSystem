[BITS 32]

; Macros to generate ISR handlers
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push dword 0          ; Dummy error code for exceptions without error code
    push dword %1         ; Interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    push dword %1         ; Interrupt number (CPU already pushed error code)
    jmp isr_common_stub
%endmacro

; Generate all 32 exception ISRs
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; Common ISR handler stub
global isr_common_stub
extern isr_handler_c

isr_common_stub:
    cli

    ; Stack now contains (top -> bottom):
    ; [int_no] (pushed by macro)
    ; [err_code] (pushed by macro or CPU)
    ; [eip]
    ; [cs]
    ; [eflags]

    pusha                  ; Push general purpose registers: edi, esi, ebp, esp, ebx, edx, ecx, eax

    push ds
    push es
    push fs
    push gs

    ; Load kernel data segment selectors into segment registers
    mov ax, 0x10           ; Kernel data segment selector (change if different)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Now ESP points to the full 'registers_t' struct in memory on the stack,
    ; which matches the order of fields in the struct:
    ; edi, esi, ebp, esp, ebx, edx, ecx, eax
    ; ds, es, fs, gs
    ; int_no, err_code
    ; eip, cs, eflags (already on stack pushed by CPU)

    mov eax, esp           ; pointer to registers_t struct
    push eax
    call isr_handler_c
    add esp, 4             ; Clean up argument from stack

    pop gs
    pop fs
    pop es
    pop ds

    popa                   ; Restore general purpose registers

    add esp, 8             ; Remove int_no and err_code from stack (2 dwords)

    sti
    iret

; --- IDT loader ---
global load_idt_asm
load_idt_asm:
    mov eax, [esp + 4]
    lidt [eax]
    ret
