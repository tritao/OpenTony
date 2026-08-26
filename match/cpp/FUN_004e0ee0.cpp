__declspec(naked) void FUN_004e0ee0()
{
    __asm {
        mov eax, dword ptr [esp+4]
        push esi
        push edi
        or ecx, -1
        mov edi, dword ptr [eax+0x10]
        xor eax, eax
        add edi, 4
        repne scasb
        not ecx
        sub edi, ecx
        mov edx, ecx
        mov esi, edi
        mov edi, 0x006a3c13
        shr ecx, 2
        rep movsd
        mov ecx, edx
        and ecx, 3
        rep movsb
        pop edi
        _emit 0xc6
        _emit 0x05
        _emit 0x12
        _emit 0x3c
        _emit 0x6a
        _emit 0x00
        _emit 0x01
        pop esi
        ret
    }
}
