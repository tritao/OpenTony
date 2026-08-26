BITS 32
org 0x004cad60

Math_Vector3sMask12:
    mov eax, 0x0fff
    and [ecx], ax
    and [ecx + 2], ax
    and [ecx + 4], ax
    ret
