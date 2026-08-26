// Fill a packed 16-bit buffer from the source stream, honoring alignment.
__declspec(naked) void FUN_004ef210()
{
    __asm {
        push ebp
        mov ebp, esp
        sub esp, 0xc
        mov eax, dword ptr [ebp+8]
        xor ecx, ecx
        push ebx
        mov edx, dword ptr [eax]
        mov cx, word ptr [eax+0x4a]
        mov dword ptr [ebp-0xc], edx
        mov edx, dword ptr [eax+4]
        mov eax, dword ptr [eax]
        cmp ecx, 8
        mov dword ptr [ebp+8], ecx
        mov dword ptr [ebp-8], edx
        mov dword ptr [ebp-4], eax
        jc short small_tail
        adc eax, 0
        mov ebx, dword ptr [ebp+0xc]
        mov eax, dword ptr [ebp-4]
        mov edx, dword ptr [ebp+0xc]
        mov ecx, dword ptr [ebp+8]
        _emit 0x0f
        _emit 0x6f
        _emit 0x45
        _emit 0xf4
        and edx, 6
        jz short aligned
        cmp edx, 2
        jne short four_byte_align
        mov word ptr [ebx], ax
        mov word ptr [ebx+2], ax
        mov word ptr [ebx+4], ax
        add ecx, -3
        add ebx, 6
        jmp short aligned
    four_byte_align:
        cmp edx, 4
        jne short two_byte_align
        mov dword ptr [ebx], eax
        add ecx, -2
        add ebx, 4
        jmp short aligned
    two_byte_align:
        mov word ptr [ebx], ax
        dec ecx
        add ebx, 2
        nop
    aligned:
        mov edx, ecx
        nop
        shr edx, 2
        nop
    block_loop:
        _emit 0x0f
        _emit 0x7f
        _emit 0x03
        nop
        add ecx, -4
        add ebx, 8
        dec edx
        jnz block_loop
        test ecx, ecx
        jz short done
    tail_loop:
        mov word ptr [ebx], ax
        nop
        add ebx, 2
        dec ecx
        jnz tail_loop
    done:
        pop ebx
        mov esp, ebp
        pop ebp
        ret
    small_tail:
        adc eax, 0
        mov ebx, dword ptr [ebp+0xc]
        mov eax, dword ptr [ebp-4]
        mov ecx, dword ptr [ebp+8]
        test ecx, ecx
        jz short small_done
    small_loop:
        mov word ptr [ebx], ax
        nop
        add ebx, 2
        dec ecx
        jnz small_loop
    small_done:
        pop ebx
        mov esp, ebp
        pop ebp
        ret
    }
}
