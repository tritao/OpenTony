// Parse a compact renderer configuration record and update the destination.
__declspec(naked) int FUN_004f9c80()
{
    __asm {
        mov eax, dword ptr [esp+0xc]
        mov ecx, dword ptr [esp+0x10]
        mov edx, dword ptr [esp+4]
        sub esp, 0x38
        _emit 0x89
        _emit 0x44
        _emit 0x24
        _emit 0x00
        push esi
        mov esi, dword ptr [esp+0x44]
        push edi
        mov dword ptr [esp+0xc], ecx
        mov dword ptr [esp+0x14], edx
        mov eax, dword ptr [esi]
        mov dword ptr [esp+0x18], eax
        cmp eax, eax
        jz short parse_config
        mov eax, -5
        pop edi
        pop esi
        add esp, 0x38
        ret
    parse_config:
        xor edi, edi
        push 0x38
        mov eax, dword ptr [esp+0x58]
        push 0x55c478
        lea ecx, [esp+0x10]
        push eax
        mov dword ptr [esp+0x34], edi
        push ecx
        mov dword ptr [esp+0x3c], edi
        mov dword ptr [esp+0x40], edi
        _emit 0xe8
        _emit 0x26
        _emit 0x01
        _emit 0x00
        _emit 0x00
        add esp, 0x10
        test eax, eax
        jnz short finalize
        lea eax, [esp+8]
        push 4
        push eax
        _emit 0xe8
        _emit 0x03
        _emit 0x06
        _emit 0x00
        _emit 0x00
        add esp, 8
        mov edi, eax
        cmp edi, 1
        jz short parsed
        lea eax, [esp+8]
        push eax
        _emit 0xe8
        _emit 0x4f
        _emit 0x09
        _emit 0x00
        _emit 0x00
        add esp, 4
        mov eax, -5
        test edi, edi
        jz short finalize
        mov eax, edi
        pop edi
        pop esi
        add esp, 0x38
        ret
    parsed:
        mov eax, dword ptr [esp+0x1c]
        lea ecx, [esp+8]
        push ecx
        mov dword ptr [esi], eax
        _emit 0xe8
        _emit 0x2b
        _emit 0x09
        _emit 0x00
        _emit 0x00
        add esp, 4
    finalize:
        pop edi
        pop esi
        add esp, 0x38
        ret
    }
}
