// Cache the three raw components of a transform state.
__declspec(naked) void FUN_004f50d0()
{
    __asm {
        mov eax, dword ptr [esp+4]
        mov ecx, dword ptr [eax]
        _emit 0x89
        _emit 0x0d
        _emit 0x48
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov edx, dword ptr [eax+4]
        _emit 0x89
        _emit 0x15
        _emit 0x4c
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov eax, dword ptr [eax+8]
        _emit 0xa3
        _emit 0x50
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        ret
    }
}
