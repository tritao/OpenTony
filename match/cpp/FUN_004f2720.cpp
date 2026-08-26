// Initialize render bounds and the 64x64 sound/render sentinel grid.
__declspec(naked) void FUN_004f2720()
{
    __asm {
        xor eax, eax
        _emit 0xa3
        _emit 0x38
        _emit 0xca
        _emit 0x54
        _emit 0x00
        _emit 0xa3
        _emit 0x34
        _emit 0xca
        _emit 0x54
        _emit 0x00
        _emit 0xa3
        _emit 0x30
        _emit 0xca
        _emit 0x54
        _emit 0x00
        _emit 0xa3
        _emit 0xac
        _emit 0x68
        _emit 0x9d
        _emit 0x02
        _emit 0xa3
        _emit 0xb0
        _emit 0x68
        _emit 0x9d
        _emit 0x02
        // Preserve the original no-op near jump over the initializers.
        _emit 0xe9
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0x00
        push esi
        push edi
        mov ecx, 0xb
        xor eax, eax
        mov edi, 0x006a9690
        rep stosd
        _emit 0xa1
        _emit 0xa0
        _emit 0x96
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0xc7
        _emit 0x05
        _emit 0xa8
        _emit 0x96
        _emit 0x6a
        _emit 0x00
        _emit 0x40
        _emit 0x00
        or al, 0x10
        _emit 0x66
        _emit 0xc7
        _emit 0x05
        _emit 0xaa
        _emit 0x96
        _emit 0x6a
        _emit 0x00
        _emit 0x40
        _emit 0x00
        _emit 0xa3
        _emit 0xa0
        _emit 0x96
        _emit 0x6a
        _emit 0x00
        mov eax, 0x006a7690
        _emit 0xa3
        _emit 0x98
        _emit 0x96
        _emit 0x6a
        _emit 0x00
        xor edi, edi
    row_loop:
        mov esi, edi
        xor ecx, ecx
        and esi, 3
    inner_loop:
        test esi, esi
        mov edx, 0x801f
        je short set_sentinel
        test cl, 3
        jne short cell_value_ready
    set_sentinel:
        mov edx, 0xffff
    cell_value_ready:
        mov word ptr [eax], dx
        inc ecx
        add eax, 2
        cmp ecx, 0x40
        jl short inner_loop
        inc edi
        cmp eax, 0x006a9690
        jl short row_loop
        pop edi
        pop esi
        ret
    }
}
