void FUN_004ebf70()
{
    __asm {
        mov eax, dword ptr [ecx+4]
        mov dword ptr [ecx], 0x00519a18
        test eax, eax
        je short done
        _emit 0xe9
        _emit 0x5e
        _emit 0x00
        _emit 0x00
        _emit 0x00
    done:
    }
}
