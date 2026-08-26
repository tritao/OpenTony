// Save a custom level after composing its normalized filename.
__declspec(naked) void FUN_004f62c0()
{
    __asm {
        sub esp, 0x40
        push ebx
        mov ebx, dword ptr [esp+0x4c]
        push ebp
        mov ebp, dword ptr [esp+0x4c]
        push esi
        push edi
        push ebx
        push ebp
        _emit 0xe8
        _emit 0x2a
        _emit 0x01
        _emit 0x00
        _emit 0x00
        add esp, 8
        test bl, bl
        jz short plain_name
        cmp byte ptr [ebp], 0x21
        jz short plain_name
        mov edi, 0x55bd2c
        or ecx, -1
        xor eax, eax
        lea edx, [esp+0x10]
        repne scasb
        not ecx
        sub edi, ecx
        mov eax, ecx
        mov esi, edi
        mov edi, edx
        lea edx, [esp+0x10]
        shr ecx, 2
        rep movsd
        mov ecx, eax
        xor eax, eax
        and ecx, 3
        rep movsb
        mov edi, ebp
        or ecx, -1
        repne scasb
        not ecx
        sub edi, ecx
        mov esi, edi
        mov ebx, ecx
        mov edi, edx
        or ecx, -1
        repne scasb
        mov ecx, ebx
        dec edi
        shr ecx, 2
        rep movsd
        mov ecx, ebx
        jmp short append_extension

    plain_name:
        mov edi, ebp
        or ecx, -1
        xor eax, eax
        lea edx, [esp+0x10]
        repne scasb
        not ecx
        sub edi, ecx
        mov eax, ecx
        mov esi, edi
        mov edi, edx
        shr ecx, 2
        rep movsd
        mov ecx, eax

    append_extension:
        and ecx, 3
        push 0x55bd08
        rep movsb
        lea ecx, [esp+0x14]
        push ecx
        _emit 0xe8
        _emit 0x1d
        _emit 0xfd
        _emit 0xff
        _emit 0xff
        lea edx, [esp+0x18]
        push edx
        push 0x55bd50
        _emit 0xe8
        _emit 0xde
        _emit 0x94
        _emit 0xfd
        _emit 0xff
        lea eax, [esp+0x20]
        push 0
        push eax
        _emit 0xe8
        _emit 0x82
        _emit 0x15
        _emit 0xff
        _emit 0xff
        mov esi, eax
        add esp, 0x18
        test esi, esi
        jl short save_error
        push 0
        push 0
        push esi
        _emit 0xe8
        _emit 0x5f
        _emit 0x16
        _emit 0xff
        _emit 0xff
        mov ecx, dword ptr [esp+0x6c]
        mov edx, dword ptr [esp+0x68]
        push ecx
        push edx
        push esi
        _emit 0xe8
        _emit 0xbf
        _emit 0x17
        _emit 0xff
        _emit 0xff
        push esi
        _emit 0xe8
        _emit 0x19
        _emit 0x18
        _emit 0xff
        _emit 0xff
        add esp, 0x1c
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x40
        ret

    save_error:
        mov edi, 0x55bce8
        or ecx, -1
        xor eax, eax
        push 0x55bd30
        repne scasb
        not ecx
        sub edi, ecx
        push 0
        mov eax, ecx
        mov esi, edi
        mov edi, 0x56881c
        _emit 0xc7
        _emit 0x05
        _emit 0x38
        _emit 0x86
        _emit 0x56
        _emit 0x00
        _emit 0xc3
        _emit 0x0a
        _emit 0x00
        _emit 0x00
        shr ecx, 2
        rep movsd
        mov ecx, eax
        and ecx, 3
        rep movsb
        _emit 0xe8
        _emit 0xf3
        _emit 0xad
        _emit 0xf0
        _emit 0xff
        add esp, 8
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x40
        ret
    }
}
