// Create the DirectDraw effect object and attach it to the parent device.
__declspec(naked) void FUN_004ed7d0()
{
    __asm {
        mov eax, dword ptr [esp+4]
        push esi
        mov esi, ecx
        push 0
        lea edx, [esi+8]
        mov dword ptr [esi+4], eax
        mov eax, dword ptr [eax+0x58]
        push edx
        lea edx, [esi+0x154]
        mov ecx, dword ptr [eax]
        push edx
        lea edx, [esi+0x18c]
        push edx
        push eax
        call dword ptr [ecx+0x48]
        test eax, eax
        jnl short effect_ok
        push 0x5fa
        push eax
        mov eax, dword ptr [esi+4]
        push 0x0054c9e4
        mov ecx, dword ptr [eax+0x54]
        _emit 0xe8
        _emit 0xaf
        _emit 0xe9
        _emit 0xff
        _emit 0xff
        _emit 0xc6
        _emit 0x86
        _emit 0xb0
        _emit 0x01
        _emit 0x00
        _emit 0x00
        _emit 0x00
    effect_ok:
        mov al, 1
        _emit 0x88
        _emit 0x86
        _emit 0xb0
        _emit 0x01
        _emit 0x00
        _emit 0x00
        pop esi
        ret 4
    }
}
