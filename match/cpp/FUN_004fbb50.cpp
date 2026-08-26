// Update the split 32-bit stream checksum over a bounded byte range.
__declspec(naked) unsigned int FUN_004fbb50()
{
    __asm {
        push ebx
        push esi
        mov ecx, dword ptr [esp+0x10]
        push edi
        mov edi, dword ptr [esp+0x10]
        push ebp
        mov esi, edi
        shr edi, 16
        and esi, 0xffff
        test ecx, ecx
        jnz have_data
        mov eax, 1
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    have_data:
        mov ebp, dword ptr [esp+0x1c]
        test ebp, ebp
        jz finish
    chunk_loop:
        cmp ebp, 0x15b0
        mov edx, ebp
        jc chunk_size
        mov edx, 0x15b0
    chunk_size:
        sub ebp, edx
        cmp edx, 0x10
        jl tail_bytes
        mov ebx, edx
        shr ebx, 4
        mov eax, ebx
        shl eax, 4
        sub edx, eax
    block_loop:
        xor eax, eax
        mov al, byte ptr [ecx]
        add esi, eax
        xor eax, eax
        add edi, esi
        mov al, byte ptr [ecx+1]
        add esi, eax
        xor eax, eax
        add edi, esi
        mov al, byte ptr [ecx+2]
        add esi, eax
        xor eax, eax
        add edi, esi
        mov al, byte ptr [ecx+3]
        add esi, eax
        xor eax, eax
        add edi, esi
        mov al, byte ptr [ecx+4]
        add esi, eax
        xor eax, eax
        add edi, esi
        mov al, byte ptr [ecx+5]
        add esi, eax
        xor eax, eax
        add edi, esi
        mov al, byte ptr [ecx+6]
        add esi, eax
        xor eax, eax
        add edi, esi
        mov al, byte ptr [ecx+7]
        add esi, eax
        xor eax, eax
        add edi, esi
        mov al, byte ptr [ecx+8]
        add esi, eax
        xor eax, eax
        add edi, esi
        mov al, byte ptr [ecx+9]
        add esi, eax
        xor eax, eax
        add edi, esi
        mov al, byte ptr [ecx+0xa]
        add esi, eax
        xor eax, eax
        add edi, esi
        mov al, byte ptr [ecx+0xb]
        add esi, eax
        xor eax, eax
        add edi, esi
        mov al, byte ptr [ecx+0xc]
        add esi, eax
        xor eax, eax
        add edi, esi
        mov al, byte ptr [ecx+0xd]
        add esi, eax
        xor eax, eax
        add edi, esi
        mov al, byte ptr [ecx+0xe]
        add esi, eax
        add edi, esi
        xor eax, eax
        mov al, byte ptr [ecx+0xf]
        add ecx, 0x10
        add esi, eax
        add edi, esi
        dec ebx
        jnz block_loop
    tail_bytes:
        test edx, edx
        jz reduce_checksum
    tail_loop:
        xor eax, eax
        mov al, byte ptr [ecx]
        inc ecx
        add esi, eax
        add edi, esi
        dec edx
        jnz tail_loop
    reduce_checksum:
        mov ebx, 0xfff1
        mov eax, esi
        sub edx, edx
        div ebx
        mov esi, edx
        mov eax, edi
        sub edx, edx
        div ebx
        mov edi, edx
        test ebp, ebp
        jnz chunk_loop
    finish:
        mov eax, edi
        pop ebp
        shl eax, 16
        pop edi
        or eax, esi
        pop esi
        pop ebx
        ret
    }
}
