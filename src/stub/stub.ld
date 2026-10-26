/* stub.ld — the font-delivered entry stub at 0x22050000. */
ENTRY(_start)
SECTIONS
{
  . = 0x22050000;
  .text : { *(.text) }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.comment) }
}
