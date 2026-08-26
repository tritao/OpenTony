BITS 32
org 0x00466090

CollisionQuery_ExecuteWrapper:
    mov eax, [esp + 0x08]
    mov ecx, [esp + 0x04]
    push eax
    push ecx
    call 0x004660b0
    add esp, byte 0x08
    db 0x33, 0xc0                ; xor eax, eax (retail 33 /r form)
    ret
