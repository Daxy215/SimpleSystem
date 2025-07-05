[BITS 16]
[ORG 0x7C00]

; %include "othercode.inc"

_start:
    cli                     ; Disable interrrupts
    xor ax, ax              ; ax = 0
    mov ds, ax              ; ds = 0
    mov es, ax              ; es = 0
    mov ss, ax              ; ss = 0
    
    ; Setup stack
    mov sp, 0x7C00              ; sp = 0x7C00
    
    ; Load kernel
    mov si, msg_loading
    call print
    call print_newline
    call load_kernal
    
    ; I forgot what this even does XD
   mov ax, 0x1000
   mov es, ax
   mov bx, 0x0000
    mov ax, [es:bx]            ; Load from address into EAX
    movzx eax, word [es:bx]
   mov eax, [es:bx]
    
    call print_hex
    call print_newline
    
    mov si, msg_loaded
    call print
    call print_newline
    
    mov ax, 0x1000
    mov es, ax
    xor bx, bx
    mov cx, 16
    .debug_loop:
        mov al, [es:bx]
        call print_hex
        mov al, ' '
        int 0x10
        inc bx
        loop .debug_loop
    call print_newline
    
    ; Switch to protected mode
    mov si, msg_gdt_load
    call print
    call print_newline
    
    cli                         ; Disable interrrupts
    lgdt [gdt_descriptor]
    
    call enable_a20             ; Enable A20
    
    mov si, msg_a20_enabled
    call print
    call print_newline
    
    ; Setup framebuffer
    call set_vesa_mode
    
    ; Set CR0.PE (Protection Enable)
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Far jump to protected mode
    jmp CODE_SEG:protected_mode

load_kernal:
    mov si, DAPACK              ; Load Disk Address Packet
    mov ah, 0x42                ; Extended Read Sectors
    mov dl, 0x80                ; Drive number (hard disk)
    int 0x13
    
    jc disk_error
    
    ret

; Needed to convert to 
enable_a20:
    call .wait_input
    mov al, 0xAD
    out 0x64, al          ; Disable keyboard
    
    call .wait_input
    mov al, 0xD0
    out 0x64, al          ; Read controller output port
    
    call .wait_output
    in al, 0x60
    push eax
    
    call .wait_input
    mov al, 0xD1
    out 0x64, al          ; Write controller output port
    
    call .wait_input
    pop eax
    or al, 2              ; Set A20 bit
    out 0x60, al
    
    call .wait_input
    mov al, 0xAE
    out 0x64, al          ; Enable keyboard
    ret

; Wait until keyboard is ready to recieve commands
.wait_input:
    in al, 0x64
    test al, 2              ; Check for bit 1
    jnz .wait_input
    ret

.wait_output:
    in al, 0x64
    test al, 1              ; Check for bit 1
    jz .wait_output
    ret

DAPACK:
    db 0x10        ; Packet size (16 bytes)
    db 0           ; Always 0
    dw 128         ; Number of sectors to read - 128
    dw 0x0000      ; Offset
    dw 0x1000      ; Segment -> 0x1000 << 4 = 0x1000 (64KBs)
    dq 1           ; Starting sector

disk_error:
    mov si, msg_disk_error
    call print
    mov al, ah  ; Error code
    call print_hex
    hlt

set_vesa_mode:
    xor ax, ax
    mov es, ax
    mov di, 0x7E00
    
    ; Get VESA mode info (INT 0x10, AX=0x4F01, CX=mode)
    mov ax, 0x4F01
    
    ; http://www.monstersoft.com/tutorial1/VESA_intro.html
    
    ; mov cx, 0x115   ; 1280x1024
    mov cx, 0x11B      ; 1280x1024  16.8M (8:8:8)
    
    int 0x10
    
    jc .fail                 ; Jump if carry set (error)
    cmp ax, 0x004F           ; Check if AX=0x004F (success)
    jne .fail
    
    ; Set VESA mode (INT 0x10, AX=0x4F02, BX=mode)
    mov ax, 0x4F02
    mov bx, 0x11B | 0x4000
    int 0x10
    
    jc .fail
    cmp ax, 0x004F
    jne .fail
    
    ; Extract values from VESA info buffer at ES:DI (0x0000:0x7E00)
    ; According to VESA VBE info block:
    ; Offset 0x12 (word): X resolution
    ; Offset 0x14 (word): Y resolution
    ; Offset 0x19 (byte): Bits per pixel
    ; Offset 0x28 (dword): Linear framebuffer address
    
    mov ax, [es:di + 0x12]    ; X resolution (word)
    mov [0x7E00], ax
    
    mov ax, [es:di + 0x14]    ; Y resolution (word)
    mov [0x7E00 + 2], ax
    
    mov al, [es:di + 0x19]    ; Bits per pixel (byte)
    mov [0x7E00 + 4], al
    mov byte [0x7E00 + 5], 0  ; Zero the high byte to keep it word-aligned
    
    mov eax, [es:di + 0x28]   ; Linear framebuffer address (dword)
    mov [0x7E00 + 6], eax
    
    ret
    
.fail:
    mov si, msg_vesa_fail
    call print
    call print_newline
    hlt
    ret

print:
    mov ah, 0x0E         ; BIOS teletype function

.next_char:
    lodsb                 ; Load next byte from string into AL
    or al, al             ; Check if end of string (null terminator)
    jz .done              ; If null terminator, end the loop
    int 0x10              ; Print the character in AL
    jmp .next_char        ; Repeat for the next character

.done:
    ret

print_hex:      ; Input: AL = byte to print (destroys AX, BX)
    pusha
    mov bh, al  ; Save original value
    
    ; Print high nibble
    mov al, bh
    shr al, 4
    call .nibble_to_ascii
    mov ah, 0x0E
    int 0x10    ; Video interrupt
    
    ; Print low nibble
    mov al, bh
    and al, 0x0F
    call .nibble_to_ascii
    mov ah, 0x0E
    int 0x10    ; Video interrupt
    
    popa
    ret

.nibble_to_ascii:
    cmp al, 9
    jbe .digit
    add al, 'A' - '9' - 1
.digit:
    add al, '0'
    ret

print_newline:
    mov ah, 0x0E
    mov al, 0x0D     ; Carriage return '\r'
    int 0x10         ; Video interrupt
    mov al, 0x0A     ; Line feed '\n'
    int 0x10         ; Video interrupt
    ret

[BITS 32]
protected_mode:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    mov esp, 0x90000 ; 0x00100000
    
    ; jmp CODE_SEG:0x0000100d
    jmp CODE_SEG:0x10000
    ; jmp CODE_SEG:0x0000
    
    hlt
    ; jmp $               ; Infinite loop?

; GDT               - TODO; Learn more about this
gdt_start:
    dq 0x0

gdt_code:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10011010b
    db 11001111b
    db 0x0
 
gdt_data:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0

gdt_end:
    
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ (gdt_code - gdt_start) | 0x08
DATA_SEG equ (gdt_data - gdt_start) | 0x10

msg_loading    db "Loading...",0
msg_vesa_fail  db "VESA fail!",0
msg_disk_error db "Disk error!", 0
msg_sectors_read db " Sectors read: ", 0

times 510 - ($ - $$) db 0

dw 0xAA55

msg_loaded     db "Kernel loaded successfully", 0
msg_gdt_load    db "Loading GDT...", 0
msg_a20_enabled db "A20 Enabled", 0
msg_jumping    db "Jumping to kernal", 0

vesa_mode_info_buffer:
    times 256 db 0

vesa_x_res:    dw 0      ; 0x7E00
vesa_y_res:    dw 0      ; 0x7E02
vesa_bpp:      dw 0      ; 0x7E04
vesa_lfb_ptr:  dd 0      ; 0x7E06
