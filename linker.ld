ENTRY(_start)

SECTIONS {
  . = 0x10000;
  
  .text :
  {
    *(.text.start)
    *(.text*)
  }

  .data : {
    *(.data)
  }

  .bss : {
    *(.bss)
    *(COMMON)
  }
}
