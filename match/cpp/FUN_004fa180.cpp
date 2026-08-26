// Reset a parser session and initialize its resource stream state.
__declspec(naked) int FUN_004fa180()
{
    __asm {
        mov edx, dword ptr [esp+4]
        push esi
        xor eax, eax
        test edx, edx
        jz short invalid
        mov esi, dword ptr [edx+0x1c]
        test esi, esi
        jz short invalid
        cmp dword ptr [edx+0x20], eax
        jz short invalid
        cmp dword ptr [edx+0x24], eax
        jz short invalid
        mov dword ptr [edx+0x14], eax
        mov dword ptr [edx+8], eax
        mov dword ptr [edx+0x2c], 2
        mov dword ptr [edx+0x18], eax
        mov dword ptr [esi+0x14], eax
        mov ecx, dword ptr [esi+8]
        mov dword ptr [esi+0x10], ecx
        cmp dword ptr [esi+0x18], eax
        jge short normalize_type
        mov dword ptr [esi+0x18], eax
    normalize_type:
        cmp dword ptr [esi+0x18], 1
        sbb eax, eax
        push esi
        and eax, 0xffffffb9
        add eax, 0x71
        mov dword ptr [esi+4], eax
        mov dword ptr [edx+0x30], 1
        mov dword ptr [esi+0x20], 0
        _emit 0xe8
        _emit 0xa0
        _emit 0x1a
        _emit 0x00
        _emit 0x00
        add esp, 4
        push esi
        _emit 0xe8
        _emit 0x07
        _emit 0x07
        _emit 0x00
        _emit 0x00
        add esp, 4
        xor eax, eax
        pop esi
        ret
    invalid:
        mov eax, -2
        pop esi
        ret
    }
}
