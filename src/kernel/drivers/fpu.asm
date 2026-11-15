bits 32

global fpu_init

fpu_init:
    mov edx, cr0 ; get cr0
    and edx, (-1) - (1 << 3 | 1 << 2) ; clear ts (bit3) and em (bit2), forces fpu access
    mov cr0, edx ; store control word

    fninit ; load defaults to fpu

    fnstsw [.testword] ; store status word
    cmp word [.testword], 0 ; compare written status to expected fpu state
    jne .nofpu ; jump if fpu hasnt written 
    jmp .hasfpu ;opposite

.nofpu:
    jmp .halt

.halt:
    cli
    hlt
    jmp .halt

.hasfpu:
    mov edx, cr0
    or edx, (1 << 5 | 1 << 1) ; set ne and mp
    and edx, ~(1 << 2) ; clear em
    mov cr0, edx
    ret

section .data
.testword: dw 0x55aa

