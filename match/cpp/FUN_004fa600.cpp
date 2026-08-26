// Flush the pending parser bytes into the caller's output buffer.
__declspec(naked) void FUN_004fa600()
{
    __asm {
        push ebx
        push esi
        mov ebx, dword ptr [esp+0xc]
        push edi
        mov ecx, dword ptr [ebx+0x1c]
        mov eax, dword ptr [ebx+0x10]
        mov edx, dword ptr [ecx+0x14]
        cmp edx, eax
        jbe short have_count
        mov edx, eax
    have_count:
        test edx, edx
        jz short done
        mov edi, dword ptr [ebx+0xc]
        mov esi, dword ptr [ecx+0x10]
        mov ecx, edx
        rep movsb
        mov eax, dword ptr [ebx+0x1c]
        add dword ptr [ebx+0xc], edx
        add dword ptr [eax+0x10], edx
        mov eax, dword ptr [ebx+0x1c]
        add dword ptr [ebx+0x14], edx
        sub dword ptr [ebx+0x10], edx
        sub dword ptr [eax+0x14], edx
        mov eax, dword ptr [ebx+0x1c]
        cmp dword ptr [eax+0x14], 0
        jnz short done
        mov ecx, dword ptr [eax+8]
        mov dword ptr [eax+0x10], ecx
    done:
        pop edi
        pop esi
        pop ebx
        ret
    }
}
