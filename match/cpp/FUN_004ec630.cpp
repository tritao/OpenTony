__declspec(naked) int FUN_004ec630()
{
    __asm {
        push esi
        mov esi, ecx
        mov eax, dword ptr [esi+0x108]
        push eax
        mov ecx, dword ptr [eax]
        call dword ptr [ecx+0x1c]
        test eax, eax
        jge short acquired
        mov ecx, dword ptr [esi+0x104]
        push 0x1f8
        push eax
        push 0x0054c878
        _emit 0xe8
        _emit 0x67
        _emit 0xfb
        _emit 0xff
        _emit 0xff
        xor al, al
        pop esi
        ret
    acquired:
        mov al, 1
        mov byte ptr [esi+0x10c], al
        pop esi
        ret
    }
}
