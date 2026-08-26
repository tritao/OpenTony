void FUN_004ec740()
{
    __asm {
        mov eax, dword ptr [ecx+0x28]
        mov dword ptr [ecx], 0x00519a20
        test eax, eax
        je short done
        _emit 0xe9
        _emit 0xde
        _emit 0x00
        _emit 0x00
        _emit 0x00
    done:
    }
}
