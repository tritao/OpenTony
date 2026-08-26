BITS 32
org 0x004cab10

Math_Vector3DecayShift:
    db 0x8b, 0xc1                 ; mov eax, ecx (retail 8B /r form)
    push esi
    mov esi, [esp + 8]
    push edi

    mov edx, [eax]
    mov cl, [esi]
    db 0x8b, 0xfa                 ; mov edi, edx
    sar edi, cl
    db 0x2b, 0xd7                 ; sub edx, edi
    mov [eax], edx

    mov edx, [eax + 4]
    mov cl, [esi + 1]
    db 0x8b, 0xfa                 ; mov edi, edx
    sar edi, cl
    db 0x2b, 0xd7                 ; sub edx, edi
    mov [eax + 4], edx

    mov edx, [eax + 8]
    mov cl, [esi + 2]
    db 0x8b, 0xfa                 ; mov edi, edx
    sar edi, cl
    db 0x2b, 0xd7                 ; sub edx, edi
    pop edi
    mov [eax + 8], edx
    pop esi
    ret 4

    times 9 nop
