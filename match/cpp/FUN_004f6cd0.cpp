// Write an INI value and flush the profile cache.
__declspec(naked) void FUN_004f6cd0()
{
    __asm {
        mov eax, dword ptr [esp+0xc]
        mov ecx, dword ptr [esp+8]
        mov edx, dword ptr [esp+4]
        push esi
        _emit 0x8b
        _emit 0x35
        _emit 0x68
        _emit 0x80
        _emit 0x51
        _emit 0x00
        push 0x029d8378
        push eax
        push ecx
        push edx
        call esi
        push 0x029d8378
        push 0
        push 0
        push 0
        call esi
        pop esi
        ret
    }
}
