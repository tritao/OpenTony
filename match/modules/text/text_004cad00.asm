BITS 32
org 0x004cad00

Math_Vector3ShiftLeftOut:
    mov eax, [esp + 0x0c]
    push esi
    push edi
    mov ecx, [eax]
    mov eax, [esp + 0x10]
    mov edx, [eax]
    mov esi, [eax + 4]
    mov edi, [eax + 8]
    mov eax, [esp + 0x0c]
    shl edx, cl
    shl esi, cl
    shl edi, cl
    db 0x8b, 0xc8                 ; mov ecx, eax
    mov [ecx], edx
    mov [ecx + 4], esi
    mov [ecx + 8], edi
    pop edi
    pop esi
    ret

    times 5 nop
