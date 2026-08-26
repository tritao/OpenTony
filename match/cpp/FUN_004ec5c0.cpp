__declspec(naked) int FUN_004ec5c0()
{
    __asm {
        push esi
        mov esi, ecx
        _emit 0xe8
        _emit 0x68
        _emit 0x00
        _emit 0x00
        _emit 0x00
        test al, al
        _emit 0x74
        _emit 0x4d
        mov eax, dword ptr [esi+0x108]
        lea edx, [esi+4]
        push edx
        push 0x100
        mov ecx, dword ptr [eax]
        push eax
        call dword ptr [ecx+0x24]
        test eax, eax
        jge short success
        cmp eax, 0x8007001e
        jne short report
        mov ecx, esi
        _emit 0xe8
        _emit 0x3d
        _emit 0x00
        _emit 0x00
        _emit 0x00
        test al, al
        jne short success
        push 0x1e3
        push 0x8007001e
        jmp short report_call
    report:
        push 0x1e9
        push eax
    report_call:
        mov ecx, dword ptr [esi+0x104]
        push 0x0054c864
        _emit 0xe8
        _emit 0xa7
        _emit 0xfb
        _emit 0xff
        _emit 0xff
        xor al, al
    done:
        pop esi
        ret
    success:
        mov al, 1
        pop esi
        ret
    }
}
