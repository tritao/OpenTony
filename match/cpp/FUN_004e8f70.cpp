__declspec(naked) void FUN_004e8f70()
{
    __asm {
        _emit 0xa1
        _emit 0x04
        _emit 0xb3
        _emit 0x54
        _emit 0x00
        push eax
        _emit 0xe8
        _emit 0x45
        _emit 0xec
        _emit 0xff
        _emit 0xff
        add esp, 4
        _emit 0xc7
        _emit 0x05
        _emit 0x04
        _emit 0xb3
        _emit 0x54
        _emit 0x00
        _emit 0xff
        _emit 0xff
        _emit 0xff
        _emit 0xff
        ret
    }
}
