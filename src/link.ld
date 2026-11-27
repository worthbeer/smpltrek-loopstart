ENTRY(loopstart_draw)
SECTIONS {
  . = 0xC3F00000;
  .text : { *(.text.loopstart_draw) *(.text.loopstart_enc) *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
