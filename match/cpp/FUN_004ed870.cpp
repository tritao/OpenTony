// Initialize the joystick/effect state block and its default effect values.
__declspec(naked) void FUN_004ed870()
{
    __asm {
        push esi
        push edi
        lea eax, [ecx+0x1a0]
        xor edi, edi
        lea edx, [ecx+0x1a8]
        lea esi, [ecx+0x130]
        mov dword ptr [ecx+0x1a4], 4
        mov dword ptr [ecx+0x1ac], edi
        mov dword ptr [ecx+0x134], edi
        mov dword ptr [ecx+0x138], edi
        mov dword ptr [ecx+0x13c], 0xc350
        mov dword ptr [ecx+0x154], 0x38
        mov dword ptr [ecx+0x158], 0x22
        mov dword ptr [ecx+0x15c], 0x1e8480
        mov dword ptr [ecx+0x160], edi
        mov dword ptr [ecx+0x164], 0x2710
        mov dword ptr [ecx+0x168], -1
        mov dword ptr [ecx+0x16c], edi
        mov dword ptr [ecx+0x170], 2
        mov dword ptr [ecx+0x174], eax
        mov dword ptr [ecx+0x178], edx
        mov dword ptr [ecx+0x17c], edi
        mov dword ptr [ecx+0x180], 0x10
        mov dword ptr [ecx+0x184], esi
        mov dword ptr [eax], edi
        mov dword ptr [edx], edi
        mov dword ptr [esi], 0x2710
        _emit 0xa1
        _emit 0x48
        _emit 0xa5
        _emit 0x51
        _emit 0x00
        add ecx, 0x18c
        pop edi
        pop esi
        mov dword ptr [ecx], eax
        _emit 0x8b
        _emit 0x15
        _emit 0x4c
        _emit 0xa5
        _emit 0x51
        _emit 0x00
        mov dword ptr [ecx+4], edx
        _emit 0xa1
        _emit 0x50
        _emit 0xa5
        _emit 0x51
        _emit 0x00
        mov dword ptr [ecx+8], eax
        _emit 0x8b
        _emit 0x15
        _emit 0x54
        _emit 0xa5
        _emit 0x51
        _emit 0x00
        mov dword ptr [ecx+0xc], edx
        ret
    }
}
