__declspec(naked) int FUN_004ec860()
{
    __asm {
        push esi
        push 0
        push 0
        push 0
        mov esi, ecx
        push 0
        _emit 0xff
        _emit 0x15
        _emit 0x50
        _emit 0x80
        _emit 0x51
        _emit 0x00
        test eax, eax
        mov dword ptr [esi+0x2c], eax
        jne short created
        mov ecx, dword ptr [esi+0x24]
        push 0x29b
        push eax
        push 0x0054c8bc
        _emit 0xe8
        _emit 0x35
        _emit 0xf9
        _emit 0xff
        _emit 0xff
        xor al, al
        pop esi
        ret
    created:
        mov ecx, dword ptr [esi+0x28]
        push eax
        push ecx
        mov edx, dword ptr [ecx]
        call dword ptr [edx+0x30]
        test eax, eax
        jge short success
        mov ecx, dword ptr [esi+0x24]
        push 0x2a2
        push eax
        push 0x0054c8bc
        _emit 0xe8
        _emit 0x10
        _emit 0xf9
        _emit 0xff
        _emit 0xff
        xor al, al
        pop esi
        ret
    success:
        mov al, 1
        pop esi
        ret
    }
}
