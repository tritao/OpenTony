// Validate and append a supported video mode descriptor.
__declspec(naked) int FUN_004f79e0()
{
    __asm {
        push ebx
        mov ebx, dword ptr [esp+0xc]
        push esi
        mov eax, dword ptr [ebx]
        cmp eax, 0x40
        jge short full
        mov esi, dword ptr [esp+0xc]
        mov ecx, eax
        shl ecx, 5
        mov edx, dword ptr [esi+0x4c]
        sub ecx, eax
        or edx, 0x40
        lea eax, [ebx+ecx*4+4]
        jnz short accepted
        pop esi
        mov eax, 1
        pop ebx
        ret 8
    accepted:
        cmp dword ptr [esi+0x54], 0x10
        jz short supported
    rejected:
        pop esi
        mov eax, 1
        pop ebx
        ret 8
    supported:
        cmp dword ptr [esi+0xc], 0x400
        ja rejected
        cmp dword ptr [esi+8], 0x300
        ja rejected
        push edi
        mov ecx, 0x1f
        mov edi, eax
        rep movsd
        mov ecx, dword ptr [eax+0x54]
        mov edx, dword ptr [eax+8]
        mov eax, dword ptr [eax+0xc]
        push ecx
        push edx
        push eax
        push 0x55c21c
        _emit 0xe8
        _emit 0x00
        _emit 0x7e
        _emit 0xfd
        _emit 0xff
        mov eax, dword ptr [ebx]
        add esp, 0x10
        inc eax
        pop edi
        mov dword ptr [ebx], eax
        pop esi
        mov eax, 1
        pop ebx
        ret 8
    full:
        pop esi
        xor eax, eax
        pop ebx
        ret 8
    }
}
