// Decode one parser token, update token statistics, and flush completed output.
__declspec(naked) unsigned char FUN_004fac60()
{
    __asm {
        sub esp, 4
        push ebx
        push esi
        mov esi, dword ptr [esp+0x10]
        push edi
        push ebp
        xor edi, edi
        mov ebp, dword ptr [esp+0x1c]
    decode_loop:
        cmp dword ptr [esi+0x6c], 0x106
        jnc have_input
        push esi
        _emit 0xe8
        _emit 0x80
        _emit 0xfe
        _emit 0xff
        _emit 0xff
        add esp, 4
        mov eax, dword ptr [esi+0x6c]
        cmp eax, 0x106
        jnc after_mode_guard
        test ebp, ebp
        jz early_return
    after_mode_guard:
        test eax, eax
        jz mode_return
    have_input:
        cmp dword ptr [esi+0x6c], 3
        jc token_state
        mov edx, dword ptr [esi+0x64]
        mov ecx, dword ptr [esi+0x30]
        xor eax, eax
        mov edi, dword ptr [esi+0x40]
        mov al, byte ptr [ecx+edx+2]
        mov cl, byte ptr [esi+0x50]
        shl edi, cl
        xor eax, edi
        mov ecx, dword ptr [esi+0x3c]
        and eax, dword ptr [esi+0x4c]
        xor edi, edi
        mov dword ptr [esi+0x40], eax
        lea eax, [ecx+eax*2]
        mov di, word ptr [eax]
        mov word ptr [eax], dx
    token_state:
        test edi, edi
        jz token_value
        mov eax, dword ptr [esi+0x24]
        mov ecx, dword ptr [esi+0x64]
        sub eax, 0x106
        sub ecx, edi
        cmp eax, ecx
        jc token_value
        cmp dword ptr [esi+0x80], 2
        jz token_value
        push edi
        push esi
        _emit 0xe8
        _emit 0xee
        _emit 0x01
        _emit 0x00
        _emit 0x00
        add esp, 8
        mov dword ptr [esi+0x58], eax
    token_value:
        mov eax, dword ptr [esi+0x58]
        cmp eax, 3
        jc literal_token
        sub al, 3
        mov edx, dword ptr [esi+0x64]
        mov ecx, dword ptr [esi+0x169c]
        sub dx, word ptr [esi+0x68]
        mov ebx, dword ptr [esi+0x1698]
        mov word ptr [ecx+ebx*2], dx
        mov ecx, dword ptr [esi+0x1690]
        dec dx
        mov ebx, dword ptr [esi+0x1698]
        mov byte ptr [ecx+ebx], al
        xor ecx, ecx
        mov cl, al
        inc dword ptr [esi+0x1698]
        xor eax, eax
        mov al, byte ptr [ecx+0x51be20]
        inc word ptr [esi+eax*4+0x490]
        cmp dx, 0x100
        jc short_length
        shr dx, 7
        xor eax, eax
        movzx ecx, dx
        mov al, byte ptr [ecx+0x51bd20]
        jmp short_length_done
    short_length:
        movzx ecx, dx
        xor eax, eax
        mov al, byte ptr [ecx+0x51bc20]
    short_length_done:
        inc word ptr [esi+eax*4+0x980]
        mov eax, dword ptr [esi+0x1694]
        sub eax, dword ptr [esi+0x1698]
        mov ecx, dword ptr [esi+0x30]
        dec eax
        cmp eax, 1
        mov eax, dword ptr [esi+0x58]
        mov dword ptr [esi+0x58], 0
        sbb ebx, ebx
        sub dword ptr [esi+0x6c], eax
        neg ebx
        add eax, dword ptr [esi+0x64]
        mov dword ptr [esi+0x64], eax
        lea edx, [ecx+eax]
        xor eax, eax
        mov cl, byte ptr [esi+0x50]
        mov al, byte ptr [edx]
        mov dword ptr [esi+0x40], eax
        shl eax, cl
        xor ecx, ecx
        mov cl, byte ptr [edx+1]
        xor eax, ecx
        and eax, dword ptr [esi+0x4c]
        mov dword ptr [esi+0x40], eax
        jmp short token_done
    literal_token:
        mov ebx, dword ptr [esi+0x30]
        mov ecx, dword ptr [esi+0x1698]
        lea eax, [esi+0x64]
        mov edx, dword ptr [esi+0x64]
        mov al, byte ptr [edx+ebx]
        mov edx, dword ptr [esi+0x169c]
        mov word ptr [edx+ecx*2], 0
        mov ecx, dword ptr [esi+0x1698]
        mov ebx, dword ptr [esi+0x1690]
        mov byte ptr [ebx+ecx], al
        xor ecx, ecx
        mov cl, al
        inc dword ptr [esi+0x1698]
        inc word ptr [esi+ecx*4+0x8c]
        mov eax, dword ptr [esi+0x1694]
        sub eax, dword ptr [esi+0x1698]
        dec eax
        cmp eax, 1
        sbb ebx, ebx
        dec dword ptr [esi+0x6c]
        neg ebx
        inc dword ptr [esi+0x64]
    token_done:
        test ebx, ebx
        jz decode_loop
        mov edx, dword ptr [esi+0x54]
        mov eax, 0
        test edx, edx
        jl flush_source_zero
        mov eax, dword ptr [esi+0x30]
        add eax, edx
    flush_source_zero:
        push 0
        mov ecx, dword ptr [esi+0x64]
        sub ecx, edx
        push ecx
        push eax
        push esi
        _emit 0xe8
        _emit 0x14
        _emit 0x12
        _emit 0x00
        _emit 0x00
        add esp, 0x10
        mov ecx, dword ptr [esi+0x64]
        mov edx, dword ptr [esi]
        mov dword ptr [esi+0x54], ecx
        push edx
        _emit 0xe8
        _emit 0xb3
        _emit 0xf7
        _emit 0xff
        _emit 0xff
        add esp, 4
        mov ecx, dword ptr [esi]
        cmp dword ptr [ecx+0x10], 0
        jnz decode_loop
        xor eax, eax
        pop ebp
        pop edi
        pop esi
        pop ebx
        add esp, 4
        ret
    early_return:
        xor eax, eax
        pop ebp
        pop edi
        pop esi
        pop ebx
        add esp, 4
        ret
    mode_return:
        mov ecx, dword ptr [esi+0x54]
        mov edx, 0
        test ecx, ecx
        jl mode_source_zero
        mov edx, dword ptr [esi+0x30]
        add edx, ecx
    mode_source_zero:
        lea eax, [ebp-4]
        cmp eax, 1
        sbb eax, eax
        neg eax
        push eax
        mov eax, dword ptr [esi+0x64]
        sub eax, ecx
        push eax
        push edx
        push esi
        _emit 0xe8
        _emit 0xb7
        _emit 0x11
        _emit 0x00
        _emit 0x00
        add esp, 0x10
        mov ecx, dword ptr [esi+0x64]
        mov edx, dword ptr [esi]
        mov dword ptr [esi+0x54], ecx
        push edx
        _emit 0xe8
        _emit 0x56
        _emit 0xf7
        _emit 0xff
        _emit 0xff
        add esp, 4
        mov ecx, dword ptr [esi]
        cmp dword ptr [ecx+0x10], 0
        jnz mode_success
        sub ebp, 4
        cmp ebp, 1
        pop ebp
        sbb eax, eax
        pop edi
        and eax, 2
        pop esi
        pop ebx
        add esp, 4
        ret
    mode_success:
        sub ebp, 4
        cmp ebp, 1
        pop ebp
        sbb eax, eax
        pop edi
        and eax, 2
        pop esi
        inc eax
        pop ebx
        add esp, 4
        ret
    }
}
