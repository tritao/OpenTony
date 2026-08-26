BITS 32
org 0x004cab50

Math_Vector3Equal:
    mov eax, [esp + 4]
    mov edx, [ecx]
    push esi
    cmp edx, [eax]
    jne short .false
    mov edx, [ecx + 4]
    mov esi, [eax + 4]
    db 0x3b, 0xd6                 ; cmp edx, esi
    jne short .false
    mov ecx, [ecx + 8]
    mov edx, [eax + 8]
    db 0x3b, 0xca                 ; cmp ecx, edx
    jne short .false
    mov eax, 1
    pop esi
    ret 4
.false:
    db 0x33, 0xc0                 ; xor eax, eax
    pop esi
    ret 4

    times 2 nop
