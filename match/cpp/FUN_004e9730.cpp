__declspec(naked) int FUN_004e9730()
{
    __asm {
        mov eax, dword ptr [esp+4]
        mov ecx, dword ptr [eax+0x20]
        cmp ecx, -2
        jne short dispatch
        mov eax, dword ptr [esp+8]
        ret
    dispatch:
        mov edx, dword ptr [eax+0x28]
        push esi
        mov esi, dword ptr [esp+0xc]
        push edi
        push eax
        mov eax, dword ptr [eax+0x2c]
        push edx
        push eax
        push esi
        call dword ptr [ecx*4+0x54b414]
        mov edi, eax
        mov eax, dword ptr [esp+0x24]
        add esp, 0x10
        test eax, eax
        je short no_release
        push esi
        _emit 0xe8
        _emit 0x01
        _emit 0x84
        _emit 0x01
        _emit 0x00
        add esp, 4
    no_release:
        mov eax, edi
        pop edi
        pop esi
        ret
    }
}
