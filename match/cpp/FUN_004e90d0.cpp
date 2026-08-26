__declspec(naked) int FUN_004e90d0()
{
    __asm {
        mov eax, dword ptr [esp+8]
        mov ecx, dword ptr [esp+4]
        push esi
        mov esi, 1
        push esi
        push eax
        push ecx
        _emit 0xe8
        _emit 0x5a
        _emit 0xfe
        _emit 0xff
        _emit 0xff
        add esp, 0x0c
        test eax, eax
        jne short failed
        mov edx, dword ptr [esp+0x18]
        mov eax, dword ptr [esp+0x14]
        mov ecx, dword ptr [esp+0x10]
        push edx
        push eax
        push ecx
        _emit 0xe8
        _emit 0xff
        _emit 0xfe
        _emit 0xff
        _emit 0xff
        add esp, 0x0c
        test eax, eax
        _emit 0x75
        _emit 0x02
        xor esi, esi
        _emit 0xe8
        _emit 0x61
        _emit 0xfe
        _emit 0xff
        _emit 0xff
        jmp short set_state
    failed:
        xor esi, esi
    set_state:
        mov eax, esi
        _emit 0xc6
        _emit 0x05
        _emit 0xbc
        _emit 0x74
        _emit 0x6a
        _emit 0x00
        _emit 0x00
        _emit 0xc7
        _emit 0x05
        _emit 0xc0
        _emit 0x74
        _emit 0x6a
        _emit 0x00
        _emit 0x04
        _emit 0x00
        _emit 0x00
        _emit 0x00
        pop esi
        ret
    }
}
