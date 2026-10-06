/* Build with MSYS2 as --32, then objcopy -O binary -j .text to CPU.COM.
 * All addresses are section-relative plus the DOS COM load offset. */
.code16
.text
start:
    pushw %cs
    popw %ds
    pushw %cs
    popw %es
    cld
    xorw %ax, %ax
    movw $1000, %cx
sum:
    addw %cx, %ax
    loop sum
    cmpw $41748, %ax
    jne failure
    movl $0x12345678, %eax
    roll $8, %eax
    cmpl $0x34567812, %eax
    jne failure
    movw $(scratch-start+0x100), %di
    movw $256, %cx
    movw $0xa55a, %ax
    rep stosw
    movw $(scratch-start+0x100), %si
    movw $256, %cx
verify:
    lodsw
    cmpw $0xa55a, %ax
    jne failure
    loop verify
    fninit
    fld1
    fld1
    faddp
    fistps (scratch-start+0x100)
    cmpw $2, (scratch-start+0x100)
    jne failure
    call modified
    cmpw $3, %ax
    jne failure
    movb $7, (immediate-start+0x100)
    call modified
    cmpw $7, %ax
    jne failure
    movw $(pass-start+0x100), %dx
    movb $9, %ah
    int $0x21
    movw $0x4c00, %ax
    int $0x21
failure:
    movw $(fail-start+0x100), %dx
    movb $9, %ah
    int $0x21
    movw $0x4c01, %ax
    int $0x21
modified:
    .byte 0xb8
immediate:
    .word 3
    ret
pass: .ascii "CPU/FPU/SMC PASS\r\n$"
fail: .ascii "CPU/FPU/SMC FAIL\r\n$"
scratch: .space 512
