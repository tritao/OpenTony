BITS 32
org 0x004cac60

Math_ScalarMulVector3Out:
    mov eax, [esp + 8]
    push esi
    mov esi, [esp + 0x10]
    push edi
    mov eax, [eax]
    mov edx, [esi + 4]
    db 0x8b, 0xc8                 ; mov ecx, eax
    imul ecx, [esi]
    imul edx, eax
    mov esi, [esi + 8]
    imul esi, eax
    mov eax, [esp + 0x0c]
    db 0x8b, 0xf8                 ; mov edi, eax
    mov [edi], ecx
    mov [edi + 4], edx
    mov [edi + 8], esi
    pop edi
    pop esi
    ret

    times 2 nop
