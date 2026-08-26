// Combine two fixed-point products and return the signed conversion polarity.
__declspec(naked) int FUN_004f5f50()
{
    __asm {
        fild dword ptr [esp+4]
        fimul dword ptr [esp+8]
        fild dword ptr [esp+0xc]
        fimul dword ptr [esp+0x10]
        _emit 0xde
        _emit 0xc1
        _emit 0xdc
        _emit 0x0d
        _emit 0x08
        _emit 0x99
        _emit 0x51
        _emit 0x00
        _emit 0xe8
        _emit 0x87
        _emit 0xa5
        _emit 0x00
        _emit 0x00
        test eax, eax
        jnz short nonzero
        ret
    nonzero:
        xor ecx, ecx
        test eax, eax
        setng cl
        dec ecx
        and ecx, 2
        dec ecx
        mov eax, ecx
        ret
    }
}
