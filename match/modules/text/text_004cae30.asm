BITS 32
org 0x004cae30

Math_Vector3sSub:
    db 0x8b, 0xc1                 ; mov eax, ecx (retail 8B /r form)
    mov ecx, [esp + 4]
    mov dx, [ecx]
    sub [eax], dx
    mov dx, [ecx + 2]
    sub [eax + 2], dx
    mov cx, [ecx + 4]
    sub [eax + 4], cx
    ret 4
