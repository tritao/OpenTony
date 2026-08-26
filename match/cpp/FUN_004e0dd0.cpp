__declspec(naked) int FUN_004e0dd0()
{
    __asm {
        mov eax, dword ptr [esp+8]
        push eax
        mov ecx, dword ptr [eax+4]
        test ecx, ecx
        jne short alternate
        mov eax, dword ptr [esp+8]
        push eax
        _emit 0xe8
        _emit 0x8a
        _emit 0x05
        _emit 0x00
        _emit 0x00
        add esp, 8
        _emit 0x8b
        _emit 0x0d
        _emit 0x1c
        _emit 0xe3
        _emit 0x56
        _emit 0x00
        _emit 0x89
        _emit 0x0d
        _emit 0x28
        _emit 0x3b
        _emit 0x6a
        _emit 0x00
        mov al, 1
        ret
    alternate:
        mov edx, dword ptr [esp+8]
        push edx
        _emit 0xe8
        _emit 0x1e
        _emit 0x00
        _emit 0x00
        _emit 0x00
        add esp, 8
        _emit 0xa1
        _emit 0x1c
        _emit 0xe3
        _emit 0x56
        _emit 0x00
        _emit 0xa3
        _emit 0x28
        _emit 0x3b
        _emit 0x6a
        _emit 0x00
        mov al, 1
        ret
    }
}
