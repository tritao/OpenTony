// Close a media descriptor and release its backing handle or stream.
__declspec(naked) int FUN_004e7bc0(int descriptor)
{
    __asm {
        push ebx
        push esi
        mov esi, dword ptr [esp+0xc]
        cmp esi, -1
        je short invalid
        _emit 0x8a
        _emit 0x86
        _emit 0x10
        _emit 0x6d
        _emit 0x6a
        _emit 0x00
        xor ebx, ebx
        cmp al, bl
        je short invalid
        cmp al, 2
        jne short buffered
        _emit 0x8b
        _emit 0x04
        _emit 0xb5
        _emit 0x1c
        _emit 0x6d
        _emit 0x6a
        _emit 0x00
        _emit 0x89
        _emit 0x1c
        _emit 0xb5
        _emit 0x1c
        _emit 0x6d
        _emit 0x6a
        _emit 0x00
        cmp eax, ebx
        _emit 0x88
        _emit 0x9e
        _emit 0x10
        _emit 0x6d
        _emit 0x6a
        _emit 0x00
        je short invalid_return
        push eax
        // Close the XA handle.
        _emit 0xe8
        _emit 0x82
        _emit 0x92
        _emit 0x01
        _emit 0x00
        add esp, 4
        inc eax
        neg eax
        sbb eax, eax
        pop esi
        neg eax
        dec eax
        pop ebx
        ret
    buffered:
        _emit 0x8b
        _emit 0x04
        _emit 0xb5
        _emit 0x94
        _emit 0x71
        _emit 0x6a
        _emit 0x00
        push eax
        // Close the buffered stream.
        _emit 0xe8
        _emit 0x58
        _emit 0x9f
        _emit 0x01
        _emit 0x00
        add esp, 4
        _emit 0x89
        _emit 0x1c
        _emit 0xb5
        _emit 0x94
        _emit 0x71
        _emit 0x6a
        _emit 0x00
        _emit 0x89
        _emit 0x1c
        _emit 0xb5
        _emit 0x6c
        _emit 0x71
        _emit 0x6a
        _emit 0x00
        _emit 0x89
        _emit 0x1c
        _emit 0xb5
        _emit 0x44
        _emit 0x6d
        _emit 0x6a
        _emit 0x00
        _emit 0x88
        _emit 0x9e
        _emit 0x10
        _emit 0x6d
        _emit 0x6a
        _emit 0x00
        pop esi
        xor eax, eax
        pop ebx
        ret
    invalid:
        push 0x0054b3ec
        _emit 0xe8
        _emit 0x0f
        _emit 0x7c
        _emit 0xfe
        _emit 0xff
        add esp, 4
    invalid_return:
        pop esi
        or eax, -1
        pop ebx
        ret
    }
}
