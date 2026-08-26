// Set effect timing and enable the associated DirectInput effect.
__declspec(naked) void FUN_004ed950()
{
    __asm {
        push esi
        push edi
        mov edi, dword ptr [esp+0x18]
        mov esi, ecx
        test edi, edi
        jnl short have_rate
        xor edi, edi
        jmp short rate_ready
    have_rate:
        jnz short rate_ready
        mov edi, 0x2710
    rate_ready:
        fld qword ptr [esp+0x10]
        _emit 0xdc
        _emit 0x0d
        _emit 0x30
        _emit 0x9a
        _emit 0x51
        _emit 0x00
        _emit 0xc7
        _emit 0x86
        _emit 0x3c
        _emit 0x01
        _emit 0x00
        _emit 0x00
        _emit 0x50
        _emit 0xc3
        _emit 0x00
        _emit 0x00
        _emit 0xe8
        _emit 0x74
        _emit 0x2b
        _emit 0x01
        _emit 0x00
        push 0x105
        mov ecx, esi
        mov dword ptr [esi+0x15c], eax
        mov dword ptr [esi+0x164], edi
        _emit 0xe8
        _emit 0x98
        _emit 0xfe
        _emit 0xff
        _emit 0xff
        mov esi, dword ptr [esi+8]
        push 0
        push 1
        push esi
        mov eax, dword ptr [esi]
        call dword ptr [eax+0x1c]
        pop edi
        pop esi
        ret 0x10
    }
}
