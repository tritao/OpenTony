// Unload the current PKR buffers after validating the archive name.
__declspec(naked) void FUN_004e9b80(const unsigned char *loaded_name)
{
    __asm {
        push esi
        push edi
        mov edi, dword ptr [esp+0xc]
        mov eax, 0x006a7578
        mov esi, edi
    compare_name:
        mov dl, byte ptr [eax]
        mov cl, dl
        cmp dl, byte ptr [esi]
        jne name_mismatch
        test cl, cl
        je short equal_name
        mov dl, byte ptr [eax+1]
        mov cl, dl
        cmp dl, byte ptr [esi+1]
        jne name_mismatch
        add eax, 2
        add esi, 2
        test cl, cl
        jne compare_name
    equal_name:
        xor eax, eax
        jmp short compare_done
    name_mismatch:
        sbb eax, eax
        sbb eax, -1
    compare_done:
        test eax, eax
        je short names_match
        push edi
        push 0x006a7578
        push 0x0054b770
        _emit 0xe8
        _emit 0x66
        _emit 0xfd
        _emit 0xff
        _emit 0xff
        add esp, 0xc
    names_match:
        mov edi, 0x0055fe10
        or ecx, -1
        xor eax, eax
        repne scasb
        not ecx
        sub edi, ecx
        mov eax, ecx
        mov esi, edi
        mov edi, 0x006a7578
        shr ecx, 2
        rep movsd
        mov ecx, eax
        and ecx, 3
        rep movsb
        _emit 0x8b
        _emit 0x0d
        _emit 0x78
        _emit 0x76
        _emit 0x6a
        _emit 0x00
        push ecx
        _emit 0xe8
        _emit 0xf5
        _emit 0x66
        _emit 0x01
        _emit 0x00
        _emit 0x8b
        _emit 0x15
        _emit 0x7c
        _emit 0x76
        _emit 0x6a
        _emit 0x00
        push edx
        _emit 0xe8
        _emit 0xe9
        _emit 0x66
        _emit 0x01
        _emit 0x00
        add esp, 8
        _emit 0xc7
        _emit 0x05
        _emit 0x78
        _emit 0x76
        _emit 0x6a
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0xc7
        _emit 0x05
        _emit 0x7c
        _emit 0x76
        _emit 0x6a
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0x00
        pop edi
        pop esi
        ret
    }
}
