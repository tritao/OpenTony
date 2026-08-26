// Emit the fixed end marker and finalize the bitstream record.
__declspec(naked) void FUN_004fbe40()
{
    __asm {
        push ebx
        push esi
        mov ax, 2
        push edi
        mov esi, dword ptr [esp+0x10]
        push ebp
        mov ecx, dword ptr [esi+0x16b8]
        lea ebx, [esi+0x16b4]
        cmp ecx, 0xd
        jng first_buffered
        shl ax, cl
        or ax, word ptr [ebx]
        mov edx, dword ptr [esi+0x14]
        mov word ptr [ebx], ax
        mov ecx, dword ptr [esi+8]
        mov byte ptr [edx+ecx], al
        mov eax, dword ptr [esi+0x14]
        mov cx, word ptr [ebx]
        inc eax
        mov edx, dword ptr [esi+8]
        mov dword ptr [esi+0x14], eax
        mov byte ptr [edx+eax], ch
        mov eax, dword ptr [esi+0x16b8]
        mov dx, 2
        mov cl, 0x10
        sub cl, al
        inc dword ptr [esi+0x14]
        shr dx, cl
        mov word ptr [ebx], dx
        sub eax, 0xd
        mov dword ptr [esi+0x16b8], eax
        jmp short first_bits_done
    first_buffered:
        shl ax, cl
        or word ptr [ebx], ax
        add ecx, 3
        mov dword ptr [esi+0x16b8], ecx
    first_bits_done:
        xor eax, eax
        mov ecx, dword ptr [esi+0x16b8]
        _emit 0x66
        _emit 0xa1
        _emit 0x2a
        _emit 0xbb
        _emit 0x51
        _emit 0x00
        mov edx, 0x10
        sub edx, eax
        cmp edx, ecx
        jnl first_no_split
        xor edi, edi
        mov ebp, dword ptr [esi+0x14]
        _emit 0x66
        _emit 0x8b
        _emit 0x3d
        _emit 0x28
        _emit 0xbb
        _emit 0x51
        _emit 0x00
        mov dx, di
        shl dx, cl
        or dx, word ptr [ebx]
        mov ecx, dword ptr [esi+8]
        mov word ptr [ebx], dx
        mov byte ptr [ebp+ecx], dl
        mov ebp, dword ptr [esi+0x14]
        mov cx, word ptr [ebx]
        lea edx, [ebp+1]
        mov ebp, dword ptr [esi+8]
        mov dword ptr [esi+0x14], edx
        mov byte ptr [ebp+edx], ch
        mov edx, dword ptr [esi+0x16b8]
        mov cl, 0x10
        add eax, edx
        sub cl, dl
        sub eax, 0x10
        shr di, cl
        mov word ptr [ebx], di
        inc dword ptr [esi+0x14]
        jmp short first_record
    first_no_split:
        _emit 0x66
        _emit 0x8b
        _emit 0x15
        _emit 0x28
        _emit 0xbb
        _emit 0x51
        _emit 0x00
        add eax, ecx
        shl dx, cl
        or word ptr [ebx], dx
    first_record:
        push esi
        mov dword ptr [esi+0x16b8], eax
        add dword ptr [esi+0x16a8], 0xa
        _emit 0xe8
        _emit 0x49
        _emit 0x17
        _emit 0x00
        _emit 0x00
        add esp, 4
        mov ecx, dword ptr [esi+0x16b8]
        mov eax, dword ptr [esi+0x16b0]
        sub eax, ecx
        add eax, 0xb
        cmp eax, 9
        jnl finish
        mov ax, 2
        cmp ecx, 0xd
        jng second_buffered
        shl ax, cl
        or ax, word ptr [ebx]
        mov edx, dword ptr [esi+0x14]
        mov word ptr [ebx], ax
        mov ecx, dword ptr [esi+8]
        mov byte ptr [edx+ecx], al
        mov eax, dword ptr [esi+0x14]
        mov cx, word ptr [ebx]
        inc eax
        mov edx, dword ptr [esi+8]
        mov dword ptr [esi+0x14], eax
        mov byte ptr [edx+eax], ch
        mov eax, dword ptr [esi+0x16b8]
        mov dx, 2
        mov cl, 0x10
        sub cl, al
        inc dword ptr [esi+0x14]
        shr dx, cl
        mov word ptr [ebx], dx
        sub eax, 0xd
        mov dword ptr [esi+0x16b8], eax
        jmp short second_bits_done
    second_buffered:
        shl ax, cl
        or word ptr [ebx], ax
        add ecx, 3
        mov dword ptr [esi+0x16b8], ecx
    second_bits_done:
        xor edi, edi
        mov ecx, dword ptr [esi+0x16b8]
        _emit 0x66
        _emit 0x8b
        _emit 0x3d
        _emit 0x2a
        _emit 0xbb
        _emit 0x51
        _emit 0x00
        mov eax, 0x10
        sub eax, edi
        cmp eax, ecx
        jnl second_no_split
        xor eax, eax
        mov ebp, dword ptr [esi+8]
        _emit 0x66
        _emit 0xa1
        _emit 0x28
        _emit 0xbb
        _emit 0x51
        _emit 0x00
        mov dx, ax
        shl dx, cl
        or dx, word ptr [ebx]
        mov ecx, dword ptr [esi+0x14]
        mov word ptr [ebx], dx
        mov byte ptr [ecx+ebp], dl
        mov ecx, dword ptr [esi+0x14]
        mov ebp, dword ptr [esi+8]
        lea edx, [ecx+1]
        mov cx, word ptr [ebx]
        mov dword ptr [esi+0x14], edx
        mov byte ptr [ebp+edx], ch
        mov edx, dword ptr [esi+0x16b8]
        mov cl, 0x10
        inc dword ptr [esi+0x14]
        sub cl, dl
        shr ax, cl
        mov word ptr [ebx], ax
        lea eax, [edx+edi]
        sub eax, 0x10
        mov dword ptr [esi+0x16b8], eax
        jmp short second_record
    second_no_split:
        _emit 0x66
        _emit 0xa1
        _emit 0x28
        _emit 0xbb
        _emit 0x51
        _emit 0x00
        shl ax, cl
        or word ptr [ebx], ax
        add ecx, edi
        mov dword ptr [esi+0x16b8], ecx
    second_record:
        add dword ptr [esi+0x16a8], 0xa
        push esi
        _emit 0xe8
        _emit 0x44
        _emit 0x16
        _emit 0x00
        _emit 0x00
        add esp, 4
    finish:
        mov dword ptr [esi+0x16b0], 7
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    }
}
