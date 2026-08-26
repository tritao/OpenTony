void FUN_004e72d0()
{
    __asm {
        _emit 0x8b
        _emit 0x0d
        _emit 0xe4
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        test ecx, ecx
        je short done
        mov eax, dword ptr [esp+4]
        mov edx, dword ptr [esp+8]
        add eax, edx
        shl eax, 0xf
        cdq
        and edx, 0x1ff
        add eax, edx
        sar eax, 9
        push eax
        push ecx
        _emit 0xff
        _emit 0x15
        _emit 0x20
        _emit 0x83
        _emit 0x51
        _emit 0x00
        _emit 0xa1
        _emit 0xe4
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        push 0x8000
        push eax
        _emit 0xff
        _emit 0x15
        _emit 0x1c
        _emit 0x83
        _emit 0x51
        _emit 0x00
    done:
    }
}
