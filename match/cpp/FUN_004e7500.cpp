// Build a media path from the configured root, separator, and relative name.
__declspec(naked) void FUN_004e7500(const char *relative_name)
{
    __asm {
        sub esp, 0x100
        or ecx, -1
        xor eax, eax
        // Preserve VC6's four-byte zero-displacement LEA encoding.
        _emit 0x8d
        _emit 0x54
        _emit 0x24
        _emit 0x00
        push ebx
        push esi
        push edi
        mov edi, 0x006a71bc
        repne scasb
        not ecx
        sub edi, ecx
        mov eax, ecx
        mov esi, edi
        mov edi, edx
        lea edx, [esp+0xc]
        shr ecx, 2
        rep movsd
        mov ecx, eax
        xor eax, eax
        and ecx, 3
        rep movsb
        mov edi, 0x00540c34
        or ecx, -1
        repne scasb
        not ecx
        sub edi, ecx
        mov esi, edi
        mov ebx, ecx
        mov edi, edx
        or ecx, -1
        repne scasb
        mov ecx, ebx
        dec edi
        shr ecx, 2
        rep movsd
        mov ecx, ebx
        lea edx, [esp+0xc]
        and ecx, 3
        rep movsb
        mov edi, dword ptr [esp+0x110]
        or ecx, -1
        repne scasb
        not ecx
        sub edi, ecx
        mov esi, edi
        mov ebx, ecx
        mov edi, edx
        or ecx, -1
        repne scasb
        mov ecx, ebx
        dec edi
        shr ecx, 2
        rep movsd
        mov ecx, ebx
        lea eax, [esp+0xc]
        and ecx, 3
        push eax
        rep movsb
        // Open the constructed path through the original media helper.
        _emit 0xe8
        _emit 0x0b
        _emit 0xaa
        _emit 0x01
        _emit 0x00
        add esp, 4
        pop edi
        pop esi
        pop ebx
        add esp, 0x100
        ret
    }
}
