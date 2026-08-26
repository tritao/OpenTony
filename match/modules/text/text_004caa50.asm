BITS 32
org 0x004caa50

Math_Vector3MulScalar:
    db 0x8b, 0xc1                 ; mov eax, ecx (retail 8B /r form)
    mov ecx, [esp + 4]
    mov edx, [ecx]
    imul edx, [eax]
    mov [eax], edx
    mov edx, [eax + 4]
    imul edx, [ecx]
    mov [eax + 4], edx
    mov edx, [eax + 8]
    imul edx, [ecx]
    mov [eax + 8], edx
    ret 4

    times 14 nop
