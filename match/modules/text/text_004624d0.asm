BITS 32
org 0x004624d0

CollisionQuery_Initialize:
    sub esp, byte 0x0c
    push ebx
    push ebp
    push esi
    mov esi, [esp + 0x1c]        ; query
    mov eax, 0x7fffffff
    push edi
    mov ecx, [esi]
    mov ebp, [esi + 0x08]
    mov [esi + 0x40], eax        ; best-distance sentinels
    mov [esi + 0x8c], eax
    mov eax, [esi + 0x0c]
    mov edi, [esi + 0x10]
    db 0x2b, 0xc1                ; sub eax, ecx (retail 2B /r form)
    mov ecx, [esi + 0x14]
    db 0x2b, 0xcd                ; sub ecx, ebp (retail 2B /r form)
    mov ebx, [esi + 0x04]
    sar ecx, byte 0x0c
    sar eax, byte 0x0c
    movsx ecx, cx
    movsx ebp, ax
    db 0x8b, 0xc1                ; mov eax, ecx (retail 8B /r form)
    mov [esp + 0x10], ebp
    imul ebp, ebp
    imul eax, ecx
    db 0x2b, 0xfb                ; sub edi, ebx (retail 2B /r form)
    db 0x33, 0xd2                ; xor edx, edx (retail 33 /r form)
    sar edi, byte 0x0c
    db 0x03, 0xe8                ; add ebp, eax (retail 03 /r form)
    mov [esi + 0x68], edx
    mov [esi + 0x80], edx
    mov dword [esi + 0x84], 0xffffffff
    mov [esi + 0x88], dl
    mov [esi + 0x89], dl
    mov [esp + 0x14], ecx
    jz near .axis_aligned

    ; The remaining fixed-point normalization and matrix construction is
    ; preserved pending review. Offset 0x78 is VA 0x00462548.
    incbin "../../original/modules/text_004624d0.bin", 0x78

.axis_aligned equ 0x004627bd
