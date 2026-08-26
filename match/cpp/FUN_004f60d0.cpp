// Resolve an asset name after normalizing its extension.
__declspec(naked) unsigned char FUN_004f60d0()
{
    __asm {
        sub esp, 0x40
        or ecx, -1
        xor eax, eax
        _emit 0x8d
        _emit 0x54
        _emit 0x24
        _emit 0x00
        push ebx
        push esi
        push edi
        mov edi, dword ptr [esp+0x50]
        xor bl, bl
        push 0x55bd08
        repne scasb
        not ecx
        sub edi, ecx
        mov eax, ecx
        mov esi, edi
        mov edi, edx
        shr ecx, 2
        rep movsd
        mov ecx, eax
        and ecx, 3
        rep movsb
        lea ecx, [esp+0x10]
        push ecx
        _emit 0xe8
        _emit 0x74
        _emit 0xff
        _emit 0xff
        _emit 0xff
        lea edx, [esp+0x14]
        push 0x549d3c
        push edx
        _emit 0xe8
        _emit 0xde
        _emit 0xb2
        _emit 0x00
        _emit 0x00
        add esp, 0x10
        test eax, eax
        jz short no_match
        push eax
        mov bl, 1
        _emit 0xe8
        _emit 0x51
        _emit 0xad
        _emit 0x00
        _emit 0x00
        add esp, 4
    no_match:
        pop edi
        mov al, bl
        pop esi
        pop ebx
        add esp, 0x40
        ret
    }
}
