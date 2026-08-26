BITS 32
org 0x004caa20

Math_Vector3Sub:
    ; Retail uses the 8B /r form instead of NASM's canonical 89 /r encoding.
    db 0x8b, 0xc1                 ; mov eax, ecx
    push esi
    mov ecx, [esp + 8]

    mov esi, [eax]
    mov edx, [ecx]
    db 0x2b, 0xf2                 ; sub esi, edx
    mov [eax], esi

    mov edx, [ecx + 4]
    mov esi, [eax + 4]
    db 0x2b, 0xf2                 ; sub esi, edx
    mov edx, [eax + 8]
    mov [eax + 4], esi

    mov ecx, [ecx + 8]
    db 0x2b, 0xd1                 ; sub edx, ecx
    pop esi
    mov [eax + 8], edx
    ret 4

    times 7 nop
