BITS 32
org 0x004caa80

Math_Vector3DivScalar:
    mov eax, [ecx]
    push esi
    mov esi, [esp + 8]
    cdq
    idiv dword [esi]
    mov [ecx], eax
    mov eax, [ecx + 4]
    cdq
    idiv dword [esi]
    mov [ecx + 4], eax
    mov eax, [ecx + 8]
    cdq
    idiv dword [esi]
    pop esi
    mov [ecx + 8], eax
    db 0x8b, 0xc1                 ; mov eax, ecx (retail 8B /r form)
    ret 4

    times 12 nop
