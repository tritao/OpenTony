BITS 32
org 0x004cab80

Math_Vector3NotEqual:
    mov eax, [esp + 4]
    mov edx, [ecx]
    push esi
    cmp edx, [eax]
    jne short .true
    mov edx, [ecx + 4]
    mov esi, [eax + 4]
    db 0x3b, 0xd6                 ; cmp edx, esi
    jne short .true
    mov ecx, [ecx + 8]
    mov edx, [eax + 8]
    db 0x3b, 0xca                 ; cmp ecx, edx
    jne short .true
    db 0x33, 0xc0                 ; xor eax, eax
    pop esi
    ret 4
.true:
    mov eax, 1
    pop esi
    ret 4

    times 2 nop
