// Shut down the game systems in the original retail order.
__declspec(naked) void FUN_004f86f0()
{
    __asm {
        push 0x55c464
        _emit 0xe8
        _emit 0x56
        _emit 0x71
        _emit 0xfd
        _emit 0xff
        _emit 0xe8
        _emit 0x71
        _emit 0x54
        _emit 0xfe
        _emit 0xff
        _emit 0xe8
        _emit 0x7c
        _emit 0x24
        _emit 0xfe
        _emit 0xff
        _emit 0xe8
        _emit 0x57
        _emit 0xc3
        _emit 0xfe
        _emit 0xff
        _emit 0xe8
        _emit 0x32
        _emit 0xea
        _emit 0xfe
        _emit 0xff
        _emit 0xe8
        _emit 0x5d
        _emit 0xec
        _emit 0xfe
        _emit 0xff
        push 1
        _emit 0xe8
        _emit 0x66
        _emit 0x88
        _emit 0xfd
        _emit 0xff
        add esp, 8
        ret
    }
}
