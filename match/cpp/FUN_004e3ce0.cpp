__declspec(naked) void FUN_004e3ce0()
{
    __asm {
        _emit 0x0f
        _emit 0xbf
        _emit 0x05
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov ecx, eax
        imul ecx, eax
        _emit 0x0f
        _emit 0xbf
        _emit 0x05
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov edx, eax
        _emit 0x89
        _emit 0x0d
        _emit 0xb8
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        imul edx, eax
        _emit 0x0f
        _emit 0xbf
        _emit 0x05
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov ecx, eax
        _emit 0x89
        _emit 0x15
        _emit 0xbc
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        imul ecx, eax
        _emit 0x89
        _emit 0x0d
        _emit 0xc0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0xe9
        _emit 0x15
        _emit 0xe4
        _emit 0xff
        _emit 0xff
    }
}
