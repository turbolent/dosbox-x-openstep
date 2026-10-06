/* One second of 440 Hz PC speaker output through the emulated PIT. */
.code16
.text
start:
    pushw %cs
    popw %ds
    inb $0x61, %al
    movb %al, (saved-start+0x100)
    movb $0xb6, %al
    outb %al, $0x43
    movw $2712, %ax
    outb %al, $0x42
    movb %ah, %al
    outb %al, $0x42
    inb $0x61, %al
    orb $3, %al
    outb %al, $0x61
    movw $0x000f, %cx
    movw $0x4240, %dx
    movb $0x86, %ah
    int $0x15
    movb (saved-start+0x100), %al
    outb %al, $0x61
    movw $0x4c00, %ax
    int $0x21
saved: .byte 0
