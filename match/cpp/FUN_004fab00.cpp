// Compact the parser input window and refill its look-ahead state.
__declspec(naked) void FUN_004fab00()
{
    __asm {
        push ebx
        push esi
        mov ebx, dword ptr [esp+0xc]
        push edi
        push ebp
        mov ebp, dword ptr [ebx+0x24]
    refill:
        mov eax, dword ptr [ebx+0x6c]
        mov ecx, dword ptr [ebx+0x64]
        mov edx, dword ptr [ebx+0x34]
        sub edx, eax
        sub edx, ecx
        jnz have_window
        test ecx, ecx
        jnz have_window
        test eax, eax
        jnz have_window
        mov edx, ebp
        jmp after_compact
    have_window:
        cmp edx, -1
        jnz have_available
        dec edx
        jmp after_compact
    have_available:
        mov eax, dword ptr [ebx+0x24]
        add eax, ebp
        sub eax, 0x106
        cmp eax, ecx
        ja after_compact
        mov ecx, ebp
        mov edi, dword ptr [ebx+0x30]
        shr ecx, 2
        mov eax, ebp
        lea esi, [edi+ebp]
        rep movsd
        mov ecx, eax
        and ecx, 3
        rep movsb
        mov esi, dword ptr [ebx+0x44]
        mov eax, dword ptr [ebx+0x3c]
        sub dword ptr [ebx+0x68], ebp
        sub dword ptr [ebx+0x64], ebp
        lea ecx, [eax+esi*2]
        sub dword ptr [ebx+0x54], ebp
    adjust_offsets:
        sub ecx, 2
        xor eax, eax
        mov ax, word ptr [ecx]
        cmp ebp, eax
        ja clear_offset
        sub ax, bp
        mov word ptr [ecx], ax
        jmp next_offset
    clear_offset:
        mov word ptr [ecx], 0
    next_offset:
        dec esi
        jnz adjust_offsets
        add edx, ebp
    after_compact:
        mov ecx, dword ptr [ebx]
        cmp dword ptr [ecx+4], 0
        jz done
        push edx
        mov eax, dword ptr [ebx+0x6c]
        add eax, dword ptr [ebx+0x64]
        add eax, dword ptr [ebx+0x30]
        push eax
        push ecx
        _emit 0xe8
        _emit 0x55
        _emit 0x00
        _emit 0x00
        _emit 0x00
        add esp, 0xc
        mov esi, dword ptr [ebx+0x6c]
        add esi, eax
        mov dword ptr [ebx+0x6c], esi
        cmp esi, 3
        jc check_limit
        mov edx, dword ptr [ebx+0x64]
        xor eax, eax
        add edx, dword ptr [ebx+0x30]
        mov cl, byte ptr [ebx+0x50]
        mov al, byte ptr [edx]
        mov dword ptr [ebx+0x40], eax
        shl eax, cl
        xor ecx, ecx
        mov cl, byte ptr [edx+1]
        xor eax, ecx
        and eax, dword ptr [ebx+0x4c]
        mov dword ptr [ebx+0x40], eax
    check_limit:
        cmp esi, 0x106
        jnc done
        mov eax, dword ptr [ebx]
        cmp dword ptr [eax+4], 0
        jnz refill
    done:
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    }
}
