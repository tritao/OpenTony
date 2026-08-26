// Append an eight-word z-buffer descriptor to the current renderer list.
__declspec(naked) int FUN_004f7980()
{
    __asm {
        push ebx
        mov ebx, dword ptr [esp+0xc]
        mov eax, dword ptr [ebx]
        cmp eax, 8
        jge short full
        push esi
        mov esi, dword ptr [esp+0xc]
        cmp dword ptr [esi+4], 0x400
        jnz short done
        shl eax, 5
        push edi
        mov ecx, 8
        lea edi, [eax+ebx+4]
        rep movsd
        mov eax, dword ptr [ebx]
        shl eax, 5
        mov ecx, dword ptr [eax+ebx+0x10]
        push ecx
        push 0x55c208
        _emit 0xe8
        _emit 0x93
        _emit 0x7e
        _emit 0xfd
        _emit 0xff
        mov eax, dword ptr [ebx]
        add esp, 8
        inc eax
        mov dword ptr [ebx], eax
        pop edi
    done:
        pop esi
        mov eax, 1
        pop ebx
        ret 8
    full:
        xor eax, eax
        pop ebx
        ret 8
    }
}
