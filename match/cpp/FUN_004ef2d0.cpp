__declspec(naked) unsigned int FUN_004ef2d0()
{
    __asm {
        _emit 0xa1
        _emit 0x8c
        _emit 0x76
        _emit 0x6a
        _emit 0x00
        test al, 7
        je short aligned
        and al, 0xf8
        add eax, 8
    aligned:
        lea ecx, [eax+0x50]
        test al, 7
        _emit 0x89
        _emit 0x0d
        _emit 0x8c
        _emit 0x76
        _emit 0x6a
        _emit 0x00
        jne short invalid
        cmp eax, 0x023f56d0
        jb short done
    invalid:
        xor eax, eax
    done:
        ret
    }
}
