// Return the fixed-point square root, with non-positive values clamped to zero.
__declspec(naked) int FUN_004f53b0()
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
        fsqrt
        _emit 0xe8
        _emit 0x1c
        _emit 0xb1
        _emit 0x00
        _emit 0x00
        add esp, 8
        ret
    }
}
