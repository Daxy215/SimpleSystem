ENTRY(_start)
SECTIONS {
    . = 0x10000;
    
    .text : ALIGN(4096) {
        *(.text.start)
        *(.text*)
    } :text
    
    .rodata : ALIGN(4096) {
        *(.rodata*)
    } :rodata
    
    .data : ALIGN(4096) {
        *(.data)
    } :data
    
    .bss : ALIGN(4096) {
        *(COMMON)
        *(.bss)
    } :bss
}

PHDRS {
    text PT_LOAD FLAGS(5);  /* R-X */
    rodata PT_LOAD FLAGS(4); /* R-- */
    data PT_LOAD FLAGS(6);  /* RW- */
    bss PT_LOAD FLAGS(6);   /* RW- */
}
