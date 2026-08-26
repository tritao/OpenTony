// Clamp a rectangle to the configured render bounds.
__declspec(naked) void FUN_004f2240()
{
    __asm {
        mov eax, dword ptr [esp+4]
        mov ecx, dword ptr [esp+0xc]
        push ebx
        push esi
        mov esi, dword ptr [esp+0x18]
        lea edx, [eax+ecx]
        mov ecx, dword ptr [esp+0x10]
        xor ebx, ebx
        add esi, ecx
        cmp eax, ebx
        _emit 0xa3
        _emit 0xc8
        _emit 0x96
        _emit 0x6a
        _emit 0x00
        _emit 0x89
        _emit 0x15
        _emit 0x20
        _emit 0x57
        _emit 0x3f
        _emit 0x02
        _emit 0x89
        _emit 0x0d
        _emit 0x28
        _emit 0x57
        _emit 0x3f
        _emit 0x02
        _emit 0x89
        _emit 0x35
        _emit 0x2c
        _emit 0x57
        _emit 0x3f
        _emit 0x02
        jnl short left_ok
        xor eax, eax
        _emit 0xa3
        _emit 0xc8
        _emit 0x96
        _emit 0x6a
        _emit 0x00
    left_ok:
        cmp ecx, ebx
        jnl short top_ok
        xor ecx, ecx
        _emit 0x89
        _emit 0x0d
        _emit 0x28
        _emit 0x57
        _emit 0x3f
        _emit 0x02
    top_ok:
        push edi
        _emit 0x8b
        _emit 0x3d
        _emit 0x30
        _emit 0xca
        _emit 0x54
        _emit 0x00
        cmp edx, edi
        jng short right_ok
        mov edx, edi
        _emit 0x89
        _emit 0x15
        _emit 0x20
        _emit 0x57
        _emit 0x3f
        _emit 0x02
    right_ok:
        _emit 0x8b
        _emit 0x3d
        _emit 0x38
        _emit 0xca
        _emit 0x54
        _emit 0x00
        cmp esi, edi
        jng short bottom_ok
        mov esi, edi
        _emit 0x89
        _emit 0x35
        _emit 0x2c
        _emit 0x57
        _emit 0x3f
        _emit 0x02
    bottom_ok:
        cmp eax, edx
        pop edi
        jg short clear_rect
        cmp ecx, esi
        jng short done
    clear_rect:
        _emit 0x89
        _emit 0x1d
        _emit 0xc8
        _emit 0x96
        _emit 0x6a
        _emit 0x00
        _emit 0x89
        _emit 0x1d
        _emit 0x20
        _emit 0x57
        _emit 0x3f
        _emit 0x02
        _emit 0x89
        _emit 0x1d
        _emit 0x28
        _emit 0x57
        _emit 0x3f
        _emit 0x02
        _emit 0x89
        _emit 0x1d
        _emit 0x2c
        _emit 0x57
        _emit 0x3f
        _emit 0x02
    done:
        pop esi
        pop ebx
        ret
    }
}
