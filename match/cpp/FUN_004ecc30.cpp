__declspec(naked) int FUN_004ecc30()
{
    __asm {
        push esi
        mov esi, ecx
        push edi
        mov eax, dword ptr [esi+0x28]
        push eax
        mov ecx, dword ptr [eax]
        call dword ptr [ecx+0x20]
        mov ecx, esi
        mov edi, eax
        _emit 0xe8
        _emit 0x1a
        _emit 0xfb
        _emit 0xff
        _emit 0xff
        test edi, edi
        jge short success
        mov ecx, dword ptr [esi+0x24]
        push 0x387
        push edi
        push 0x0054c920
        _emit 0xe8
        _emit 0x63
        _emit 0xf5
        _emit 0xff
        _emit 0xff
        pop edi
        xor al, al
        pop esi
        ret
    success:
        mov byte ptr [esi+0x50], 0
        pop edi
        mov al, 1
        pop esi
        ret
    }
}
