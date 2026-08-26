__declspec(naked) int FUN_004ec8c0()
{
    __asm {
        sub esp, 0x14
        push esi
        mov esi, ecx
        lea edx, [esp+4]
        mov dword ptr [esp+4], 0x14
        mov eax, dword ptr [esi+0x28]
        mov dword ptr [esp+8], 0x10
        mov dword ptr [esp+0xc], 0
        mov dword ptr [esp+0x10], 0
        mov dword ptr [esp+0x14], 0x20
        mov ecx, dword ptr [eax]
        push edx
        push 1
        push eax
        call dword ptr [ecx+0x18]
        test eax, eax
        jge short success
        mov ecx, dword ptr [esi+0x24]
        push 0x2c0
        push eax
        push 0x0054c8dc
        _emit 0xe8
        _emit 0xab
        _emit 0xf8
        _emit 0xff
        _emit 0xff
        xor al, al
        pop esi
        add esp, 0x14
        ret
    success:
        mov al, 1
        pop esi
        add esp, 0x14
        ret
    }
}
