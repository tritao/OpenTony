__declspec(naked) unsigned int FUN_004e4650()
{
    __asm {
        _emit 0xa1
        _emit 0xe4
        _emit 0xac
        _emit 0x54
        _emit 0x00
        xor edx, edx
        mov ecx, eax
        mov dl, ah
        and ecx, 0xff
        xor eax, eax
        _emit 0x66
        _emit 0xa1
        _emit 0x1e
        _emit 0x3f
        _emit 0x6a
        _emit 0x00
        _emit 0x89
        _emit 0x0d
        _emit 0x44
        _emit 0xb1
        _emit 0x56
        _emit 0x00
        _emit 0x8b
        _emit 0x0d
        _emit 0x1c
        _emit 0x3f
        _emit 0x6a
        _emit 0x00
        _emit 0x89
        _emit 0x15
        _emit 0xb0
        _emit 0xb2
        _emit 0x56
        _emit 0x00
        shl eax, 0x10
        and ecx, 0xffff
        or eax, ecx
        ret
    }
}
