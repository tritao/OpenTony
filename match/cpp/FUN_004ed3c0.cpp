__declspec(naked) int FUN_004ed3c0()
{
    __asm {
        push esi
        mov esi, ecx
        mov eax, dword ptr [esi+0x58]
        push eax
        mov ecx, dword ptr [eax]
        call dword ptr [ecx+0x20]
        test eax, eax
        jge short success
        mov ecx, dword ptr [esi+0x54]
        push 0x54f
        push eax
        push 0x0054c9cc
        _emit 0xe8
        _emit 0xdd
        _emit 0xed
        _emit 0xff
        _emit 0xff
        xor al, al
        pop esi
        ret
    success:
        mov byte ptr [esi+0xec], 0
        mov al, 1
        pop esi
        ret
    }
}
