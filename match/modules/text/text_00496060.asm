BITS 32
org 0x00496060

Skater_PositionWritePath:
    sub esp, 0xa8
    push esi
    db 0x8b, 0xf1                 ; mov esi, ecx (retail 8B /r form)
    mov eax, [esi + 0x3200]
    test eax, eax
    jz short .collision_resolution

    ; Fast path: collision resolution is disabled, so commit XYZ directly.
    mov eax, [esp + 0xb0]
    mov ecx, [esp + 0xb4]
    mov edx, [esp + 0xb8]
    mov [esi + 0x08], eax
    mov [esi + 0x0c], ecx
    mov [esi + 0x10], edx
    pop esi
    add esp, 0xa8
    ret 0x0c

.collision_resolution:
    ; Preserved until the collision-query ABI and eight fallback candidates
    ; have semantic tests. Offset 0x3b is VA 0x0049609b.
    incbin "../../original/modules/text_00496060.bin", 0x3b
