// Initialize parser token tables and embedded stream pointers.
__declspec(naked) void FUN_004fbc80()
{
    __asm {
        _emit 0xe8
        _emit 0x7b
        _emit 0x00
        _emit 0x00
        _emit 0x00
        mov ecx, dword ptr [esp+4]
        xor edx, edx
        lea eax, [ecx+0x8c]
        mov dword ptr [ecx+0x16a8], edx
        mov dword ptr [ecx+0xb18], 0x55c590
        mov dword ptr [ecx+0xb24], 0x55c5a8
        mov word ptr [ecx+0x16b4], dx
        mov dword ptr [ecx+0xb10], eax
        lea eax, [ecx+0x980]
        mov dword ptr [ecx+0x16b8], edx
        push ecx
        mov dword ptr [ecx+0xb1c], eax
        mov dword ptr [ecx+0xb30], 0x55c5c0
        mov dword ptr [ecx+0x16b0], 8
        lea eax, [ecx+0xa74]
        mov dword ptr [ecx+0xb28], eax
        _emit 0xe8
        _emit 0x20
        _emit 0x00
        _emit 0x00
        _emit 0x00
        add esp, 4
        ret
    }
}
