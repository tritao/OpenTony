__declspec(naked) int FUN_004e83e0()
{
    __asm {
        mov eax, dword ptr [esp+4]
        sub esp, 8
        test eax, eax
        jg short positive
        xor eax, eax
        add esp, 8
        ret
    positive:
        _emit 0x89
        _emit 0x44
        _emit 0x24
        _emit 0x00
        mov dword ptr [esp+4], 0
        _emit 0xdf
        _emit 0x6c
        _emit 0x24
        _emit 0x00
        _emit 0xd9
        _emit 0xfa
        _emit 0xe8
        _emit 0xec
        _emit 0x80
        _emit 0x01
        _emit 0x00
        add esp, 8
        ret
    }
}
