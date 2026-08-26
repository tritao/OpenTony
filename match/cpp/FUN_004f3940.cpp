// Re-enable active sound slots after the sound system has been initialized.
__declspec(naked) void FUN_004f3940()
{
    __asm {
        _emit 0xa1
        _emit 0x60
        _emit 0x83
        _emit 0x9d
        _emit 0x02
        test eax, eax
        jz short done
        push esi
        push edi
        xor edi, edi
        mov esi, 0x029d6940
    scan:
        lea eax, [esi-0x20]
        test eax, eax
        jz short next
        test byte ptr [esi], 1
        jz short next
        push -1
        push -1
        push edi
        _emit 0xe8
        _emit 0x98
        _emit 0xf8
        _emit 0xff
        _emit 0xff
        add esp, 0xc
    next:
        add esi, 0x28
        inc edi
        _emit 0x81
        _emit 0xfe
        _emit 0x40
        _emit 0x7d
        _emit 0x9d
        _emit 0x02
        jl short scan
        pop edi
        pop esi
    done:
        ret
    }
}
