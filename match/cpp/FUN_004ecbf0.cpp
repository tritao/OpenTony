__declspec(naked) int FUN_004ecbf0()
{
    __asm {
        push esi
        mov esi, ecx
        push edi
        mov eax, dword ptr [esi+0x28]
        push eax
        mov ecx, dword ptr [eax]
        call dword ptr [ecx+0x1c]
        mov ecx, esi
        mov edi, eax
        _emit 0xe8
        _emit 0x5a
        _emit 0xfb
        _emit 0xff
        _emit 0xff
        test edi, edi
        jge short success
        mov ecx, dword ptr [esi+0x24]
        push 0x375
        push edi
        push 0x0054c90c
        _emit 0xe8
        _emit 0xa3
        _emit 0xf5
        _emit 0xff
        _emit 0xff
        pop edi
        xor al, al
        pop esi
        ret
    success:
        mov al, 1
        pop edi
        mov byte ptr [esi+0x50], al
        pop esi
        ret
    }
}
