// Release the active sound slot and clear its ownership flag.
__declspec(naked) void FUN_004f23d0()
{
    __asm {
        _emit 0xa1
        _emit 0xa8
        _emit 0x68
        _emit 0x9d
        _emit 0x02
        test eax, eax
        jl short no_active_slot
        push 1
        push eax
        _emit 0xe8
        _emit 0x7f
        _emit 0x01
        _emit 0x00
        _emit 0x00
        add esp, 8
    no_active_slot:
        _emit 0xe8
        _emit 0x17
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0xc6
        _emit 0x05
        _emit 0xb9
        _emit 0x68
        _emit 0x9d
        _emit 0x02
        _emit 0x00
        ret
    }
}
