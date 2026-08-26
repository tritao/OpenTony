BITS 32
org 0x004caab0

Math_Vector3ShiftRight:
    mov edx, [esp + 4]
    db 0x8b, 0xc1                 ; mov eax, ecx (retail 8B /r form)
    push esi
    mov ecx, [edx]
    mov esi, [eax]
    sar esi, cl
    mov [eax], esi
    mov ecx, [edx]
    mov esi, [eax + 4]
    sar esi, cl
    mov [eax + 4], esi
    mov ecx, [edx]
    mov edx, [eax + 8]
    pop esi
    sar edx, cl
    mov [eax + 8], edx
    ret 4

    times 9 nop
