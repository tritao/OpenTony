__declspec(naked) int FUN_004e8f40()
{
    __asm {
        mov eax, dword ptr [esp+0x0c]
        mov ecx, dword ptr [esp+8]
        push 0
        push eax
        push ecx
        _emit 0xe8
        _emit 0x5f
        _emit 0xe6
        _emit 0xff
        _emit 0xff
        add esp, 0x0c
        _emit 0xa3
        _emit 0x04
        _emit 0xb3
        _emit 0x54
        _emit 0x00
        inc eax
        neg eax
        sbb eax, eax
        and al, 0xfb
        add eax, 5
        ret
    }
}
