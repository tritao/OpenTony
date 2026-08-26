BITS 32
org 0x004cac90

Math_Vector3DivScalarOut:
    mov eax, [esp + 0x0c]
    push ebx
    push esi
    mov esi, [esp + 0x10]
    mov ecx, [eax]
    push edi
    mov eax, [esi]
    cdq
    idiv ecx
    db 0x8b, 0xf8                 ; mov edi, eax
    mov eax, [esi + 4]
    cdq
    idiv ecx
    db 0x8b, 0xd8                 ; mov ebx, eax
    mov eax, [esi + 8]
    cdq
    idiv ecx
    mov ecx, [esp + 0x10]
    db 0x8b, 0xd1                 ; mov edx, ecx
    mov [edx], edi
    pop edi
    pop esi
    mov [edx + 4], ebx
    pop ebx
    mov [edx + 8], eax
    db 0x8b, 0xc1                 ; mov eax, ecx
    ret

    times 10 nop
