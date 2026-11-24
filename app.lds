OUTPUT_ARCH( "riscv" )

SECTIONS 
{
  . = 0x00001000;
  .text : {
    *(.text._start)
    *(.text .text.*)
  }
  . = ALIGN(0x1000);
  .rodata : 
  { 
    *(.rdata)
    *(.rodata .rodata.* .srodata .srodata.*)
  }
  . = ALIGN(0x1000);
  .data : { 
	*(.data .data.* .sdata .sdata.*)
  }
  . = ALIGN(0x1000);
  .bss : {
	*(.bss .bss.* .sbss .sbss.* COMMON)
  }
  .debug : { *(.debug) }

  . = ALIGN(0x1000);
  .brk_area :
  {
    __brk_start0 = .;
    . = . + 4096 * 64;
    __brk_end = .;
  }

  . = ALIGN(0x1000);
  .mmap_area :
  {
    __mmap_start0 = .;
    . = . + 4096 * 64;
    __mmap_end = .;
  }
  _end = .;
}
