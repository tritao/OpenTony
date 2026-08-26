// Reset parser stream cursors, dimensions, and mode state.
__declspec(naked) void FUN_004fa8f0()
{
    __asm {
        mov edx, dword ptr [esp+4]
        push ebx
        push esi
        push edi
        mov eax, dword ptr [edx+0x24]
        mov ecx, dword ptr [edx+0x44]
        add eax, eax
        mov ebx, dword ptr [edx+0x3c]
        xor esi, esi
        mov dword ptr [edx+0x34], eax
        mov word ptr [ebx+ecx*2-2], si
        xor eax, eax
        mov ecx, dword ptr [edx+0x44]
        mov edi, dword ptr [edx+0x3c]
        lea ebx, [ecx*2-2]
        mov ecx, ebx
        shr ecx, 2
        rep stosd
        mov ecx, ebx
        and ecx, 3
        rep stosb
        mov eax, dword ptr [edx+0x7c]
        lea ebx, [eax+eax*2]
        xor eax, eax
        mov ax, word ptr [ebx*4+0x51b552]
        lea ecx, [ebx*4]
        mov dword ptr [edx+0x78], eax
        xor eax, eax
        mov ax, word ptr [ecx+0x51b550]
        mov dword ptr [edx+0x84], eax
        xor eax, eax
        mov ax, word ptr [ecx+0x51b554]
        mov dword ptr [edx+0x88], eax
        xor eax, eax
        mov ax, word ptr [ecx+0x51b556]
        mov dword ptr [edx+0x74], eax
        mov dword ptr [edx+0x64], esi
        mov eax, 2
        mov dword ptr [edx+0x54], esi
        mov dword ptr [edx+0x6c], esi
        mov dword ptr [edx+0x70], eax
        pop edi
        mov dword ptr [edx+0x58], eax
        mov dword ptr [edx+0x60], esi
        mov dword ptr [edx+0x40], esi
        pop esi
        pop ebx
        ret
    }
}
