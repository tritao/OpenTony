// Allocate and initialize the parser resource stream from a template record.
__declspec(naked) int FUN_004fa710()
{
    __asm {
        sub esp, 4
        push ebx
        push esi
        mov esi, dword ptr [esp+0x14]
        push edi
        push ebp
        test esi, esi
        mov edx, dword ptr [esi+0x1c]
        mov dword ptr [esp+0x10], edx
        jz invalid
        mov ebp, dword ptr [esp+0x18]
        test ebp, ebp
        jz invalid
        test edx, edx
        jz invalid
        mov edi, ebp
        mov ecx, 0xe
        rep movsd
        push 0x16bc
        mov eax, dword ptr [ebp+0x28]
        push 1
        push eax
        call dword ptr [ebp+0x20]
        add esp, 0xc
        mov ebx, eax
        test ebx, ebx
        jnz allocated
        mov eax, -4
        pop ebp
        pop edi
        pop esi
        pop ebx
        add esp, 4
        ret
    allocated:
        mov edi, ebx
        mov esi, dword ptr [esp+0x10]
        mov ecx, 0x5af
        push 2
        mov dword ptr [ebp+0x1c], ebx
        rep movsd
        mov dword ptr [ebx], ebp
        mov eax, dword ptr [ebx+0x24]
        mov ecx, dword ptr [ebp+0x28]
        push eax
        push ecx
        call dword ptr [ebp+0x20]
        add esp, 0xc
        mov dword ptr [ebx+0x30], eax
        mov ecx, dword ptr [ebx+0x24]
        mov edx, dword ptr [ebp+0x28]
        push 2
        push ecx
        push edx
        call dword ptr [ebp+0x20]
        add esp, 0xc
        mov dword ptr [ebx+0x38], eax
        mov ecx, dword ptr [ebx+0x44]
        mov edx, dword ptr [ebp+0x28]
        push 2
        push ecx
        push edx
        call dword ptr [ebp+0x20]
        add esp, 0xc
        mov dword ptr [ebx+0x3c], eax
        mov ecx, dword ptr [ebx+0x1694]
        mov edx, dword ptr [ebp+0x28]
        push 4
        push ecx
        push edx
        call dword ptr [ebp+0x20]
        add esp, 0xc
        mov dword ptr [ebx+8], eax
        mov edi, dword ptr [ebx+0x30]
        test edi, edi
        jz failed
        cmp dword ptr [ebx+0x38], 0
        jz failed
        cmp dword ptr [ebx+0x3c], 0
        jz failed
        test eax, eax
        jz failed
        mov ebp, dword ptr [ebx+0x24]
        mov edx, dword ptr [esp+0x10]
        add ebp, ebp
        mov esi, dword ptr [edx+0x30]
        mov ecx, ebp
        shr ecx, 2
        rep movsd
        mov ecx, ebp
        and ecx, 3
        rep movsb
        mov ebp, dword ptr [ebx+0x24]
        mov edi, dword ptr [ebx+0x38]
        add ebp, ebp
        mov edx, dword ptr [esp+0x10]
        mov ecx, ebp
        mov esi, dword ptr [edx+0x38]
        shr ecx, 2
        rep movsd
        mov ecx, ebp
        and ecx, 3
        rep movsb
        mov ebp, dword ptr [ebx+0x44]
        mov edi, dword ptr [ebx+0x3c]
        add ebp, ebp
        mov edx, dword ptr [esp+0x10]
        mov ecx, ebp
        mov esi, dword ptr [edx+0x3c]
        shr ecx, 2
        rep movsd
        mov ecx, ebp
        and ecx, 3
        rep movsb
        mov ebp, dword ptr [ebx+0xc]
        mov edi, dword ptr [ebx+8]
        mov edx, dword ptr [esp+0x10]
        mov ecx, ebp
        shr ecx, 2
        mov esi, dword ptr [edx+8]
        rep movsd
        mov ecx, ebp
        and ecx, 3
        rep movsb
        mov edx, dword ptr [esp+0x10]
        mov ecx, dword ptr [edx+0x10]
        sub ecx, dword ptr [edx+8]
        add ecx, dword ptr [ebx+8]
        mov dword ptr [ebx+0x10], ecx
        mov ecx, dword ptr [ebx+0x1694]
        and ecx, -2
        add ecx, eax
        mov dword ptr [ebx+0x169c], ecx
        mov ecx, dword ptr [ebx+0x1694]
        lea eax, [ecx+ecx*2]
        add eax, dword ptr [ebx+8]
        lea ecx, [ebx+0x8c]
        lea edx, [ebx+0x980]
        mov dword ptr [ebx+0x1690], eax
        lea eax, [ebx+0xa74]
        pop ebp
        pop edi
        mov dword ptr [ebx+0xb10], ecx
        pop esi
        mov dword ptr [ebx+0xb1c], edx
        mov dword ptr [ebx+0xb28], eax
        xor eax, eax
        pop ebx
        add esp, 4
        ret
    failed:
        push ebp
        _emit 0xe8
        _emit 0x87
        _emit 0xfd
        _emit 0xff
        _emit 0xff
        add esp, 4
        mov eax, -4
        pop ebp
        pop edi
        pop esi
        pop ebx
        add esp, 4
        ret
    invalid:
        mov eax, -2
        pop ebp
        pop edi
        pop esi
        pop ebx
        add esp, 4
        ret
    }
}
