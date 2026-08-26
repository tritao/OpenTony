__declspec(naked) void FUN_004e1490()
{
    __asm {
        push esi
        push edi
        mov edi, dword ptr [esp+0x0c]
        or ecx, -1
        xor eax, eax
        push 0x006a15a8
        repne scasb
        not ecx
        sub edi, ecx
        push 0
        mov eax, ecx
        mov esi, edi
        mov edi, 0x006a15a8
        push 0x501
        shr ecx, 2
        rep movsd
        mov ecx, eax
        and ecx, 3
        rep movsb
        _emit 0x8b
        _emit 0x0d
        _emit 0xe8
        _emit 0x3d
        _emit 0x6a
        _emit 0x00
        push ecx
        _emit 0xff
        _emit 0x15
        _emit 0x48
        _emit 0x82
        _emit 0x51
        _emit 0x00
        pop edi
        pop esi
        ret
    }
}
