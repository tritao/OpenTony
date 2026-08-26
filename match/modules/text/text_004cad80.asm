BITS 32
org 0x004cad80

Math_Vector3sSnapNearZero:
    mov ax, [ecx]
    db 0x33, 0xd2                 ; xor edx, edx
    cmp ax, strict word -1
    jl short .y
    cmp ax, strict word 1
    jg short .y
    mov [ecx], dx
.y:
    mov ax, [ecx + 2]
    cmp ax, strict word -1
    jl short .z
    cmp ax, strict word 1
    jg short .z
    mov [ecx + 2], dx
.z:
    mov ax, [ecx + 4]
    cmp ax, strict word -1
    jl short .done
    cmp ax, strict word 1
    jg short .done
    mov [ecx + 4], dx
.done:
    ret
