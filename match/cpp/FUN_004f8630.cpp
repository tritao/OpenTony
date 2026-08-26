// Append a DirectDraw device record and copy its owned display name.
__declspec(naked) int FUN_004f8630()
{
    __asm {
        push ebp
        mov ebp, dword ptr [esp+0x14]
        mov eax, dword ptr [ebp]
        cmp eax, 8
        jge full
        shl eax, 8
        push ebx
        push esi
        lea ebx, [eax+ebp+4]
        mov eax, dword ptr [esp+0x10]
        test eax, eax
        push edi
        jz short zero_descriptor
        mov edx, dword ptr [eax]
        mov ecx, ebx
        mov dword ptr [ecx], edx
        mov edx, dword ptr [eax+4]
        mov dword ptr [ecx+4], edx
        mov edx, dword ptr [eax+8]
        mov dword ptr [ecx+8], edx
        mov eax, dword ptr [eax+0xc]
        mov dword ptr [ecx+0xc], eax
        jmp short copy_name
    zero_descriptor:
        mov edx, ebx
        xor ecx, ecx
        mov dword ptr [edx], ecx
        mov dword ptr [edx+4], ecx
        mov dword ptr [edx+8], ecx
        mov dword ptr [edx+0xc], ecx
    copy_name:
        mov esi, dword ptr [esp+0x18]
        or ecx, -1
        mov edi, esi
        xor eax, eax
        repne scasb
        not ecx
        push ecx
        _emit 0xe8
        _emit 0x23
        _emit 0x95
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
        push 0x55c450
        _emit 0xe8
        _emit 0x87
        _emit 0x71
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
        ret 0x14
    full:
        xor eax, eax
        pop ebp
        ret 0x14
    }
}
