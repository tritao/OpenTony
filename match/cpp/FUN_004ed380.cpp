__declspec(naked) int FUN_004ed380()
{
    __asm {
        push esi
        mov esi, ecx
        mov eax, dword ptr [esi+0x58]
        push eax
        mov ecx, dword ptr [eax]
        call dword ptr [ecx+0x1c]
        test eax, eax
        jge short success
        mov ecx, dword ptr [esi+0x54]
        push 0x53e
        push eax
        push 0x0054c9b4
        _emit 0xe8
        _emit 0x1d
        _emit 0xee
        _emit 0xff
        _emit 0xff
        xor al, al
        pop esi
        ret
    success:
        mov al, 1
        mov byte ptr [esi+0xec], al
        pop esi
        ret
    }
}
