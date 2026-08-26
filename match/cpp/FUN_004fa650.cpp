// Release parser-owned resources after validating the record type.
__declspec(naked) int FUN_004fa650()
{
    __asm {
        push esi
        push edi
        mov esi, dword ptr [esp+0xc]
        test esi, esi
        jz short invalid
        mov eax, dword ptr [esi+0x1c]
        test eax, eax
        jz short invalid
        mov edi, dword ptr [eax+4]
        cmp edi, 0x2a
        jz short valid_type
        cmp edi, 0x71
        jz short valid_type
        cmp edi, 0x29a
        jz short valid_type
        mov eax, -2
        pop edi
        pop esi
        ret
    valid_type:
        mov eax, dword ptr [eax+8]
        test eax, eax
        jz short release_3c
        push eax
        mov eax, dword ptr [esi+0x28]
        push eax
        call dword ptr [esi+0x24]
        add esp, 8
    release_3c:
        mov eax, dword ptr [esi+0x1c]
        mov eax, dword ptr [eax+0x3c]
        test eax, eax
        jz short release_38
        push eax
        mov eax, dword ptr [esi+0x28]
        push eax
        call dword ptr [esi+0x24]
        add esp, 8
    release_38:
        mov eax, dword ptr [esi+0x1c]
        mov eax, dword ptr [eax+0x38]
        test eax, eax
        jz short release_30
        push eax
        mov eax, dword ptr [esi+0x28]
        push eax
        call dword ptr [esi+0x24]
        add esp, 8
    release_30:
        mov eax, dword ptr [esi+0x1c]
        mov eax, dword ptr [eax+0x30]
        test eax, eax
        jz short release_header
        push eax
        mov eax, dword ptr [esi+0x28]
        push eax
        call dword ptr [esi+0x24]
        add esp, 8
    release_header:
        mov eax, dword ptr [esi+0x1c]
        mov ecx, dword ptr [esi+0x28]
        push eax
        sub edi, 0x71
        push ecx
        call dword ptr [esi+0x24]
        mov dword ptr [esi+0x1c], 0
        add esp, 8
        cmp edi, 1
        sbb eax, eax
        pop edi
        and eax, -3
        pop esi
        ret
    invalid:
        mov eax, -2
        pop edi
        pop esi
        ret
    }
}
