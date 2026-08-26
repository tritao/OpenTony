BITS 32
org 0x004cae10

Math_Vector3sAdd:
    db 0x8b, 0xc1                 ; mov eax, ecx (retail 8B /r form)
    mov ecx, [esp + 4]
    mov dx, [ecx]
    add [eax], dx
    mov dx, [ecx + 2]
    add [eax + 2], dx
    mov cx, [ecx + 4]
    add [eax + 4], cx
    ret 4
