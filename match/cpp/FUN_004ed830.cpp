__declspec(naked) int FUN_004ed830()
{
    __asm {
        mov edx, dword ptr [esp+4]
        push esi
        mov esi, ecx
        push edx
        mov eax, dword ptr [esi+8]
        lea edx, [esi+0x154]
        push edx
        push eax
        mov ecx, dword ptr [eax]
        call dword ptr [ecx+0x18]
        test eax, eax
        jge short success
        push 0x60a
        push eax
        mov eax, dword ptr [esi+4]
        push 0x0054c9fc
        mov ecx, dword ptr [eax+0x54]
        _emit 0xe8
        _emit 0x5e
        _emit 0xe9
        _emit 0xff
        _emit 0xff
        xor al, al
        pop esi
        ret 4
    success:
        mov al, 1
        pop esi
        ret 4
    }
}
