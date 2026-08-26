// Append a D3D device record and copy its diagnostic name into owned storage.
__declspec(naked) int FUN_004f78f0()
{
    __asm {
        push ebp
        mov ebp, dword ptr [esp+0x14]
        mov eax, dword ptr [ebp]
        cmp eax, 8
        jge short full
        shl eax, 8
        push ebx
        push esi
        mov esi, dword ptr [esp+0x18]
        lea ebx, [eax+ebp+4]
        push edi
        mov ecx, 0x3b
        lea edi, [ebx+0x10]
        xor eax, eax
        rep movsd
        mov esi, dword ptr [esp+0x14]
        or ecx, -1
        mov edi, esi
        repne scasb
        not ecx
        push ecx
        _emit 0xe8
        _emit 0x8a
        _emit 0xa2
        _emit 0x00
        _emit 0x00
        mov edx, eax
        mov edi, esi
        or ecx, -1
        xor eax, eax
        mov dword ptr [ebx+0xfc], edx
        repne scasb
        not ecx
        sub edi, ecx
        mov eax, ecx
        mov esi, edi
        mov edi, edx
        shr ecx, 2
        rep movsd
        mov ecx, eax
        and ecx, 3
        rep movsb
        mov ecx, dword ptr [ebx+0xfc]
        push ecx
        push 0x55c1f4
        _emit 0xe8
        _emit 0xee
        _emit 0x7e
        _emit 0xfd
        _emit 0xff
        mov eax, dword ptr [ebp]
        add esp, 0xc
        inc eax
        pop edi
        pop esi
        mov dword ptr [ebp], eax
        pop ebx
        mov eax, 1
        pop ebp
        ret 0x10
    full:
        xor eax, eax
        pop ebp
        ret 0x10
    }
}
