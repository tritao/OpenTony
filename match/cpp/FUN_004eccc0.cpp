void FUN_004eccc0()
{
    __asm {
        mov eax, dword ptr [ecx+0x58]
        mov dword ptr [ecx], 0x00519a24
        test eax, eax
        je short done
        _emit 0xe9
        _emit 0x7e
        _emit 0x02
        _emit 0x00
        _emit 0x00
    done:
    }
}
