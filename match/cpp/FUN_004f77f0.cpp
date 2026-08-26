// Dump the selected D3D capability fields through the diagnostic logger.
__declspec(naked) void FUN_004f77f0()
{
    __asm {
        push esi
        push 0x54c24c
        _emit 0xe8
        _emit 0x55
        _emit 0x80
        _emit 0xfd
        _emit 0xff
        push 0x55c1d8
        _emit 0xe8
        _emit 0x4b
        _emit 0x80
        _emit 0xfd
        _emit 0xff
        push 0x55c1bc
        _emit 0xe8
        _emit 0x41
        _emit 0x80
        _emit 0xfd
        _emit 0xff
        mov esi, dword ptr [esp+0x14]
        mov eax, dword ptr [esi+0x74]
        push eax
        push 0x55c19c
        _emit 0xe8
        _emit 0x2f
        _emit 0x80
        _emit 0xfd
        _emit 0xff
        mov ecx, dword ptr [esi+0x78]
        push ecx
        push 0x55c17c
        _emit 0xe8
        _emit 0x21
        _emit 0x80
        _emit 0xfd
        _emit 0xff
        mov edx, dword ptr [esi+0x7c]
        push edx
        push 0x55c15c
        _emit 0xe8
        _emit 0x13
        _emit 0x80
        _emit 0xfd
        _emit 0xff
        mov eax, dword ptr [esi+0x80]
        push eax
        push 0x55c13c
        _emit 0xe8
        _emit 0x02
        _emit 0x80
        _emit 0xfd
        _emit 0xff
        mov ecx, dword ptr [esi+0x84]
        push ecx
        push 0x55c11c
        _emit 0xe8
        _emit 0xf1
        _emit 0x7f
        _emit 0xfd
        _emit 0xff
        mov edx, dword ptr [esi+0x88]
        push edx
        push 0x55c0fc
        _emit 0xe8
        _emit 0xe0
        _emit 0x7f
        _emit 0xfd
        _emit 0xff
        mov eax, dword ptr [esi+0x8c]
        push eax
        push 0x55c0dc
        _emit 0xe8
        _emit 0xcf
        _emit 0x7f
        _emit 0xfd
        _emit 0xff
        mov ecx, dword ptr [esi+0x90]
        add esp, 0x44
        push ecx
        push 0x55c0bc
        _emit 0xe8
        _emit 0xbb
        _emit 0x7f
        _emit 0xfd
        _emit 0xff
        push 0x55c0a4
        _emit 0xe8
        _emit 0xb1
        _emit 0x7f
        _emit 0xfd
        _emit 0xff
        push 0x55c08c
        _emit 0xe8
        _emit 0xa7
        _emit 0x7f
        _emit 0xfd
        _emit 0xff
        mov edx, dword ptr [esi+0x5c]
        push edx
        push 0x55c054
        _emit 0xe8
        _emit 0x99
        _emit 0x7f
        _emit 0xfd
        _emit 0xff
        mov eax, dword ptr [esi+0x5c]
        and eax, 2
        push eax
        push 0x55c01c
        _emit 0xe8
        _emit 0x88
        _emit 0x7f
        _emit 0xfd
        _emit 0xff
        mov ecx, dword ptr [esi+0x5c]
        and ecx, 0x20
        push ecx
        push 0x55bfe4
        _emit 0xe8
        _emit 0x77
        _emit 0x7f
        _emit 0xfd
        _emit 0xff
        push 0x54c24c
        _emit 0xe8
        _emit 0x6d
        _emit 0x7f
        _emit 0xfd
        _emit 0xff
        add esp, 0x2c
        pop esi
        ret
    }
}
