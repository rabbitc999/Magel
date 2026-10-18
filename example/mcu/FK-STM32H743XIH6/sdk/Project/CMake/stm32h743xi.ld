ENTRY(Reset_Handler)
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 2048K
  AXIRAM (rwx) : ORIGIN = 0x24000000, LENGTH = 512K
  DTCM (rw) : ORIGIN = 0x20000000, LENGTH = 128K
  GUI_CAPTURE (rw) : ORIGIN = 0xC0300000, LENGTH = 1M
  GUI_SHADOW (rw) : ORIGIN = 0xC0400000, LENGTH = 1M
  SDRAM_HEAP (rw) : ORIGIN = 0xC0500000, LENGTH = 25M
  SDRAM_MAP (rw) : ORIGIN = 0xC1E00000, LENGTH = 2M
}
_estack = ORIGIN(DTCM) + LENGTH(DTCM);
SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
  .text : { *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) . = ALIGN(4); } > FLASH
  .ARM.extab : { *(.ARM.extab*) } > FLASH
  .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
  .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); } > FLASH
  .init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array)) PROVIDE_HIDDEN(__init_array_end = .); } > FLASH
  .fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array)) PROVIDE_HIDDEN(__fini_array_end = .); } > FLASH
  _sidata = LOADADDR(.data);
  .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > AXIRAM AT> FLASH
  /* Keil absolute SDRAM sections must be consumed before the .bss wildcard.
     SDRAM is initialized by Board_Init, so startup must not touch these ranges. */
  .gui_capture (NOLOAD) : { . += LENGTH(GUI_CAPTURE); } > GUI_CAPTURE
  .gui_shadow (NOLOAD) : { . += LENGTH(GUI_SHADOW); } > GUI_SHADOW
  .external_heap (NOLOAD) : { *(.bss.ARM.__at_0xC0500000) } > SDRAM_HEAP
  .external_map (NOLOAD) : { *(.bss.ARM.__at_0xC1E00000) } > SDRAM_MAP
  .bss (NOLOAD) : { . = ALIGN(32); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(32); _ebss = .; } > AXIRAM
  .newlib_heap (NOLOAD) : { . = ALIGN(8); __heap_start = .; . += 64K; __heap_end = .; } > DTCM
  ASSERT(__heap_end <= _estack - 32K, "Insufficient DTCM stack")
  /DISCARD/ : { *(.comment) *(.note*) }
}
