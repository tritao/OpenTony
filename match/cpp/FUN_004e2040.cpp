__declspec(naked) int FUN_004e2040()
{
    __asm {
        mov eax, dword ptr [esp+8]
        mov ecx, dword ptr [esp+4]
        push eax
        push 0x004e1aa0
        push 0
        push 0x67
        push ecx
        _emit 0xff
        _emit 0x15
        _emit 0x44
        _emit 0x82
        _emit 0x51
        _emit 0x00
        neg eax
        sbb eax, eax
        and eax, 0x7788fee8
        add eax, 0x88770118
        ret
    }
}
