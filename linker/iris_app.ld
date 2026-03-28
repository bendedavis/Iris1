/* Iris application linker script (STM32F030R8T6)
 * Reserve 8 KB for future audio bootloader.
 */

MEMORY
{
    BOOT (rx)  : ORIGIN = 0x08000000, LENGTH = 8K
    FLASH (rx) : ORIGIN = 0x08002000, LENGTH = 56K
    /* Reserve first 0x100 bytes for remapped vector table (Cortex-M0 SRAM remap). */
    RAM (rwx)  : ORIGIN = 0x20000100, LENGTH = 0x1F00
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame*)
    } > FLASH

    .ARM.exidx :
    {
        *(.ARM.exidx*)
    } > FLASH

    . = ALIGN(4);
    _etext = .;

    .data : AT (_etext)
    {
        _sidata = LOADADDR(.data);
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM
}
