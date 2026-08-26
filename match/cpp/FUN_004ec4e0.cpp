__declspec(naked) void FUN_004ec4e0()
{
    __asm {
        mov eax, dword ptr [ecx+0x108]
        mov dword ptr [ecx], 0x00519a1c
        test eax, eax
        je short done
        _emit 0xe9
        _emit 0x9b
        _emit 0x00
        _emit 0x00
        _emit 0x00
    done:
        ret
    }
}
