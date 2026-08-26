// Append a variable-width token and emit its associated payload record.
__declspec(naked) void FUN_004fbd80()
{
    __asm {
        mov edx, dword ptr [esp+4]
        push ebx
        push esi
        push edi
        mov ecx, dword ptr [edx+0x16b8]
        cmp ecx, 0xd
        jng buffered_bits
        mov si, word ptr [esp+0x1c]
        mov eax, dword ptr [edx+8]
        mov bx, si
        shl bx, cl
        or bx, word ptr [edx+0x16b4]
        mov ecx, dword ptr [edx+0x14]
        mov word ptr [edx+0x16b4], bx
        mov byte ptr [ecx+eax], bl
        mov ecx, dword ptr [edx+0x14]
        mov bx, word ptr [edx+0x16b4]
        mov edi, dword ptr [edx+8]
        lea eax, [ecx+1]
        mov cl, 0x10
        mov dword ptr [edx+0x14], eax
        mov byte ptr [edi+eax], bh
        mov eax, dword ptr [edx+0x16b8]
        sub cl, al
        inc dword ptr [edx+0x14]
        shr si, cl
        mov word ptr [edx+0x16b4], si
        sub eax, 0xd
        mov dword ptr [edx+0x16b8], eax
        jmp short emit_record
    buffered_bits:
        mov eax, dword ptr [esp+0x1c]
        shl ax, cl
        or word ptr [edx+0x16b4], ax
        add ecx, 3
        mov dword ptr [edx+0x16b8], ecx
    emit_record:
        mov eax, dword ptr [edx+0x16a8]
        mov ecx, dword ptr [esp+0x18]
        add eax, 0xa
        push 1
        and eax, 0xfffffff8
        push ecx
        mov dword ptr [edx+0x16a8], eax
        lea ebx, [eax+ecx*8+0x20]
        mov eax, dword ptr [esp+0x1c]
        mov dword ptr [edx+0x16a8], ebx
        push eax
        push edx
        _emit 0xe8
        _emit 0x2f
        _emit 0x19
        _emit 0x00
        _emit 0x00
        add esp, 0x10
        pop edi
        pop esi
        pop ebx
        ret
    }
}
