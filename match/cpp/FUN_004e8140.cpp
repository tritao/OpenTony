// Build the same rotation matrix with the alternate angle order.
__declspec(naked) int *FUN_004e8140(short *angles, int *matrix)
{
    __asm {
        push esi
        mov esi, dword ptr [esp+0xc]
        mov ecx, esi
        xor eax, eax
        push edi
        mov edi, dword ptr [esp+0xc]
        mov dword ptr [ecx], eax
        push esi
        mov dword ptr [ecx+4], eax
        mov dword ptr [ecx+8], eax
        mov dword ptr [ecx+0xc], eax
        mov word ptr [ecx+0x10], ax
        mov eax, 0x1000
        mov word ptr [esi], ax
        mov word ptr [esi+8], ax
        mov word ptr [esi+0x10], ax
        movsx edx, word ptr [edi+4]
        push edx
        _emit 0xe8
        _emit 0xe8
        _emit 0xfd
        _emit 0xff
        _emit 0xff
        movsx eax, word ptr [edi+2]
        push esi
        push eax
        _emit 0xe8
        _emit 0x5d
        _emit 0xfc
        _emit 0xff
        _emit 0xff
        movsx ecx, word ptr [edi]
        push esi
        push ecx
        _emit 0xe8
        _emit 0xd3
        _emit 0xfa
        _emit 0xff
        _emit 0xff
        add esp, 0x18
        mov eax, esi
        pop edi
        pop esi
        ret
    }
}
