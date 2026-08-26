BITS 32
org 0x004cad30

Math_Vector3NegateOut:
    mov eax, [esp + 4]
    mov edx, [ecx]
    push esi
    mov esi, [ecx + 4]
    mov ecx, [ecx + 8]
    push edi
    db 0x8b, 0xf8                 ; mov edi, eax
    neg edx
    mov [edi], edx
    neg esi
    mov [edi + 4], esi
    neg ecx
    mov [edi + 8], ecx
    pop edi
    pop esi
    ret 4
