// Advance the game clock from the multimedia timer when no pause gate is set.
__declspec(naked) void FUN_004f5ff0()
{
    __asm {
        _emit 0xe8
        _emit 0x3b
        _emit 0x4b
        _emit 0xfe
        _emit 0xff
        test al, al
        jnz short done
        push esi
        _emit 0xe8
        _emit 0xcc
        _emit 0xcc
        _emit 0x00
        _emit 0x00
        _emit 0x8b
        _emit 0x0d
        _emit 0x6c
        _emit 0x83
        _emit 0x9d
        _emit 0x02
        mov esi, eax
        test ecx, ecx
        jnz short have_previous
        mov ecx, esi
    have_previous:
        mov eax, esi
        sub eax, ecx
        lea eax, [eax+eax*2]
        lea ecx, [eax+eax*4]
        mov eax, 0x10624dd3
        shl ecx, 2
        imul ecx
        sar edx, 6
        mov eax, edx
        shr eax, 0x1f
        add edx, eax
        test edx, edx
        jng short store_previous
        _emit 0x8b
        _emit 0x0d
        _emit 0x1c
        _emit 0xe3
        _emit 0x56
        _emit 0x00
        add ecx, edx
        _emit 0x89
        _emit 0x0d
        _emit 0x1c
        _emit 0xe3
        _emit 0x56
        _emit 0x00
        _emit 0xa1
        _emit 0x04
        _emit 0x1c
        _emit 0x56
        _emit 0x00
        test eax, eax
        jnz short store_previous
        _emit 0xa1
        _emit 0xe0
        _emit 0xa8
        _emit 0x56
        _emit 0x00
        test eax, eax
        jnz short store_previous
        _emit 0xa1
        _emit 0x20
        _emit 0xe3
        _emit 0x56
        _emit 0x00
        add eax, edx
        _emit 0xa3
        _emit 0x20
        _emit 0xe3
        _emit 0x56
        _emit 0x00
    store_previous:
        _emit 0x89
        _emit 0x35
        _emit 0x6c
        _emit 0x83
        _emit 0x9d
        _emit 0x02
        pop esi
    done:
        ret
    }
}
