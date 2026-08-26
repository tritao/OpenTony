__declspec(naked) int FUN_004ebf90()
{
    __asm {
        mov eax, dword ptr [esp+4]
        push esi
        mov esi, ecx
        push 0
        mov ecx, dword ptr [esp+0x10]
        lea edx, [esi+4]
        mov dword ptr [esi+0x10], eax
        push edx
        push 0x700
        push eax
        mov dword ptr [esi+0x14], ecx
        _emit 0xe8
        _emit 0x30
        _emit 0xc9
        _emit 0x00
        _emit 0x00
        test eax, eax
        jge short success
        push 0x77
        push eax
        push 0x0054c514
        mov ecx, esi
        _emit 0xe8
        _emit 0xfb
        _emit 0x01
        _emit 0x00
        _emit 0x00
        xor al, al
        pop esi
        ret 8
    success:
        mov al, 1
        pop esi
        ret 8
    }
}
