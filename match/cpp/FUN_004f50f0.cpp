// Cache the integer components of a fixed-point vector.
__declspec(naked) void FUN_004f50f0()
{
    __asm {
        mov edx, dword ptr [esp+4]
        mov eax, dword ptr [edx]
        mov ecx, dword ptr [edx+4]
        mov edx, dword ptr [edx+8]
        sar eax, 0xc
        sar ecx, 0xc
        sar edx, 0xc
        _emit 0xa3
        _emit 0xa0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x89
        _emit 0x0d
        _emit 0xa4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x89
        _emit 0x15
        _emit 0xa8
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        ret
    }
}
