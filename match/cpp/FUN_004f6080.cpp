// Remove a filename extension and append the supplied suffix in place.
__declspec(naked) void FUN_004f6080()
{
    __asm {
        push ebx
        mov ebx, dword ptr [esp+8]
        push esi
        push edi
        push 0x2e
        push ebx
        _emit 0xe8
        _emit 0x21
        _emit 0xba
        _emit 0x00
        _emit 0x00
        add esp, 8
        test eax, eax
        jz short no_extension
        mov byte ptr [eax], 0
    no_extension:
        mov edi, dword ptr [esp+0x14]
        or ecx, -1
        xor eax, eax
        repne scasb
        not ecx
        sub edi, ecx
        mov esi, edi
        mov edx, ecx
        mov edi, ebx
        or ecx, -1
        repne scasb
        mov ecx, edx
        dec edi
        shr ecx, 2
        rep movsd
        mov ecx, edx
        and ecx, 3
        rep movsb
        pop edi
        pop esi
        pop ebx
        ret
    }
}
