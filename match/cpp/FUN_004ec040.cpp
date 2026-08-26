__declspec(naked) int FUN_004ec040()
{
    __asm {
        push esi
        mov esi, ecx
        push 0x101
        push esi
        mov eax, dword ptr [esi+4]
        push 0x004ec160
        push 4
        push eax
        mov ecx, dword ptr [eax]
        call dword ptr [ecx+0x10]
        test eax, eax
        jge short success
        push 0xaa
        push eax
        push 0x0054c548
        mov ecx, esi
        _emit 0xe8
        _emit 0x51
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
