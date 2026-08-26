__declspec(naked) int FUN_004ec080()
{
    __asm {
        push esi
        mov esi, ecx
        push 1
        push esi
        mov eax, dword ptr [esi+4]
        push 0x004ec100
        push 0
        push eax
        mov ecx, dword ptr [eax]
        call dword ptr [ecx+0x10]
        test eax, eax
        jge short success
        push 0xbf
        push eax
        push 0x0054c56c
        mov ecx, esi
        _emit 0xe8
        _emit 0x14
        _emit 0x01
        _emit 0x00
        _emit 0x00
        xor al, al
        pop esi
        ret
    success:
        mov al, 1
        pop esi
        ret
    }
}
