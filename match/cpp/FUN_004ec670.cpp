__declspec(naked) int FUN_004ec670()
{
    __asm {
        push esi
        mov esi, ecx
        mov eax, dword ptr [esi+0x108]
        push eax
        mov ecx, dword ptr [eax]
        call dword ptr [ecx+0x20]
        test eax, eax
        jge short acquired
        mov ecx, dword ptr [esi+0x104]
        push 0x20a
        push eax
        push 0x0054c890
        _emit 0xe8
        _emit 0x27
        _emit 0xfb
        _emit 0xff
        _emit 0xff
        xor al, al
        pop esi
        ret
    acquired:
        mov byte ptr [esi+0x10c], 0
        mov al, 1
        pop esi
        ret
    }
}
