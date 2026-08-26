// Load a 12-word transform and signed translation into shared math state.
__declspec(naked) void FUN_004f4fe0()
{
    __asm {
        mov eax, dword ptr [esp+4]
        mov cx, word ptr [eax]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x10
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [eax+2]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x12
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [eax+4]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x14
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [eax+6]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x16
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [eax+8]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x18
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [eax+0xa]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x1a
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [eax+0xc]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x1c
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [eax+0xe]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x1e
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [eax+0x10]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x20
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        movsx edx, word ptr [eax+0x12]
        _emit 0x89
        _emit 0x15
        _emit 0xa0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        movsx ecx, word ptr [eax+0x14]
        _emit 0x89
        _emit 0x0d
        _emit 0xa4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        movsx edx, word ptr [eax+0x16]
        _emit 0x89
        _emit 0x15
        _emit 0xa8
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        ret
    }
}
