// Resolve a packed sound identifier and convert its duration to fixed-point.
__declspec(naked) unsigned int FUN_004f3720()
{
    __asm {
        sub esp, 8
        mov ecx, dword ptr [esp+0xc]
        _emit 0x8b
        _emit 0x15
        _emit 0x60
        _emit 0x83
        _emit 0x9d
        _emit 0x02
        mov eax, ecx
        and ecx, 0xffff
        sar eax, 0x10
        test edx, edx
        push esi
        mov dword ptr [esp+4], 0
        jnz short sound_system_ready
        xor eax, eax
        pop esi
        add esp, 8
        ret
    sound_system_ready:
        lea ecx, [ecx+ecx*4]
        lea edx, [eax+ecx*2]
        _emit 0x8b
        _emit 0x04
        _emit 0x95
        _emit 0x20
        _emit 0x69
        _emit 0x9d
        _emit 0x02
        lea edx, [esp+4]
        push edx
        push eax
        mov ecx, dword ptr [eax]
        call dword ptr [ecx+0x20]
        mov esi, eax
        test esi, esi
        je short convert_duration
        push 0x8bb
        push 0x0055bba0
        push esi
        _emit 0xe8
        _emit 0x36
        _emit 0xc2
        _emit 0xfd
        _emit 0xff
        push 1
        _emit 0xe8
        _emit 0xff
        _emit 0xd7
        _emit 0xfd
        _emit 0xff
        add esp, 0x10
        push esi
        _emit 0xe8
        _emit 0xaf
        _emit 0xdc
        _emit 0x00
        _emit 0x00
    convert_duration:
        mov eax, dword ptr [esp+4]
        mov dword ptr [esp+8], 0
        mov dword ptr [esp+4], eax
        fild qword ptr [esp+4]
        _emit 0xd8
        _emit 0x0d
        _emit 0x70
        _emit 0x9a
        _emit 0x51
        _emit 0x00
        _emit 0xd8
        _emit 0x0d
        _emit 0x8c
        _emit 0x99
        _emit 0x51
        _emit 0x00
        _emit 0xe8
        _emit 0x45
        _emit 0xcd
        _emit 0x00
        _emit 0x00
        pop esi
        add esp, 8
        ret
    }
}
