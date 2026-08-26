// Convert two fixed-point vectors to integer vectors and normalize the axes.
__declspec(naked) void FUN_004f40d0()
{
    __asm {
        mov eax, dword ptr [esp+4]
        mov ecx, dword ptr [esp+8]
        push esi
        mov edx, dword ptr [eax]
        sar edx, 0xc
        mov dword ptr [ecx], edx
        mov edx, dword ptr [eax+4]
        sar edx, 0xc
        mov dword ptr [ecx+4], edx
        mov edx, dword ptr [eax+8]
        sar edx, 0xc
        mov dword ptr [ecx+8], edx
        mov esi, dword ptr [eax+0xc]
        mov edx, dword ptr [esp+0x10]
        sar esi, 0xc
        mov dword ptr [edx], esi
        mov esi, dword ptr [eax+0x10]
        sar esi, 0xc
        mov dword ptr [edx+4], esi
        mov eax, dword ptr [eax+0x14]
        sar eax, 0xc
        mov dword ptr [edx+8], eax
        mov eax, dword ptr [esp+0x14]
        push eax
        push edx
        push ecx
        mov dword ptr [eax], 0
        _emit 0xe8
        _emit 0x2e
        _emit 0xff
        _emit 0xff
        _emit 0xff
        add esp, 0xc
        pop esi
        ret
    }
}
