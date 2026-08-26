// Build and delete the normalized filename for a custom level asset.
__declspec(naked) void FUN_004f6400()
{
    __asm {
        sub esp, 0x100
        or ecx, -1
        xor eax, eax
        _emit 0x8d
        _emit 0x54
        _emit 0x24
        _emit 0x00
        push ebx
        push esi
        push edi
        mov edi, 0x6a71bc
        repne scasb
        not ecx
        sub edi, ecx
        mov eax, ecx
        mov esi, edi
        mov edi, edx
        _emit 0x8d
        _emit 0x54
        _emit 0x24
        _emit 0x0c
        shr ecx, 2
        rep movsd
        mov ecx, eax
        xor eax, eax
        and ecx, 3
        rep movsb
        mov edi, 0x540c34
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
        mov al, byte ptr [esp+0x114]
        mov edx, dword ptr [esp+0x110]
        mov ecx, ebx
        and ecx, 3
        test al, al
        rep movsb
        jz short append_name
        cmp byte ptr [edx], 0x21
        jz short append_name
        mov edi, 0x55bd2c
        or ecx, -1
        xor eax, eax
        push ebp
        repne scasb
        not ecx
        sub edi, ecx
        _emit 0x8d
        _emit 0x5c
        _emit 0x24
        _emit 0x10
        mov esi, edi
        mov ebp, ecx
        mov edi, ebx
        or ecx, -1
        repne scasb
        mov ecx, ebp
        dec edi
        shr ecx, 2
        rep movsd
        mov ecx, ebp
        pop ebp
        and ecx, 3
        rep movsb
    append_name:
        mov edi, edx
        or ecx, -1
        xor eax, eax
        _emit 0x8d
        _emit 0x5c
        _emit 0x24
        _emit 0x0c
        repne scasb
        not ecx
        sub edi, ecx
        push 0x55bd08
        mov esi, edi
        mov edx, ecx
        mov edi, ebx
        or ecx, -1
        repne scasb
        mov ecx, edx
        dec edi
        shr ecx, 2
        rep movsd
        mov ecx, edx
        lea eax, [esp+0x10]
        and ecx, 3
        push eax
        rep movsb
        _emit 0xe8
        _emit 0xa2
        _emit 0xfb
        _emit 0xff
        _emit 0xff
        lea ecx, [esp+0x14]
        push ecx
        push 0x55bd6c
        _emit 0xe8
        _emit 0x63
        _emit 0x93
        _emit 0xfd
        _emit 0xff
        lea edx, [esp+0x1c]
        push edx
        _emit 0xe8
        _emit 0xa9
        _emit 0xba
        _emit 0x00
        _emit 0x00
        add esp, 0x14
        pop edi
        pop esi
        pop ebx
        add esp, 0x100
        ret
    }
}
