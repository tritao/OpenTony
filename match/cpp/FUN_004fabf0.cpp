// Copy up to the requested number of bytes from the parser input stream.
__declspec(naked) unsigned int FUN_004fabf0()
{
    __asm {
        push ebx
        push esi
        mov ebx, dword ptr [esp+0xc]
        push edi
        mov eax, dword ptr [esp+0x18]
        push ebp
        mov ecx, dword ptr [ebx+4]
        cmp ecx, eax
        mov ebp, ecx
        jna have_count
        mov ebp, eax
    have_count:
        test ebp, ebp
        jnz copy_data
        xor eax, eax
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    copy_data:
        sub ecx, ebp
        mov eax, dword ptr [ebx+0x1c]
        mov dword ptr [ebx+4], ecx
        cmp dword ptr [eax+0x18], 0
        jnz have_buffer
        push ebp
        mov eax, dword ptr [ebx]
        push eax
        mov ecx, dword ptr [ebx+0x30]
        push ecx
        _emit 0xe8
        _emit 0x23
        _emit 0x0f
        _emit 0x00
        _emit 0x00
        add esp, 0xc
        mov dword ptr [ebx+0x30], eax
    have_buffer:
        mov ecx, ebp
        mov eax, ebp
        shr ecx, 2
        mov edi, dword ptr [esp+0x18]
        mov esi, dword ptr [ebx]
        rep movsd
        mov ecx, eax
        and ecx, 3
        rep movsb
        add dword ptr [ebx], ebp
        add dword ptr [ebx+8], ebp
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    }
}
