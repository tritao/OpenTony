__declspec(naked) void FUN_004ec930()
{
    __asm {
        mov edx, dword ptr [esp+4]
        mov eax, dword ptr [ecx+4]
        push ebx
        mov ebx, dword ptr [esp+0x14]
        push esi
        mov esi, dword ptr [esp+0x14]
        push edi
        mov edi, dword ptr [esp+0x14]
        cmp eax, edx
        mov dword ptr [ecx+0x14], edx
        mov dword ptr [ecx+0x1c], esi
        mov dword ptr [ecx+0x18], edi
        mov dword ptr [ecx+0x20], ebx
        jge short min_done
        mov eax, edx
        jmp short min_store
    min_done:
        cmp eax, esi
        jle short min_store
        mov eax, esi
    min_store:
        mov dword ptr [ecx+4], eax
        mov eax, dword ptr [ecx+8]
        cmp eax, edi
        jge short max_done
        mov dword ptr [ecx+8], edi
        pop edi
        pop esi
        pop ebx
        ret 0x10
    max_done:
        cmp eax, ebx
        jle short no_store
        pop edi
        mov dword ptr [ecx+8], ebx
        pop esi
        pop ebx
        ret 0x10
    no_store:
        pop edi
        pop esi
        mov dword ptr [ecx+8], eax
        pop ebx
        ret 0x10
    }
}
