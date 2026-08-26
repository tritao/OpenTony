__declspec(naked) int FUN_004e8f20()
{
    __asm {
        mov eax, dword ptr [esp+8]
        push eax
        _emit 0xe8
        _emit 0xd6
        _emit 0xe5
        _emit 0xff
        _emit 0xff
        add esp, 4
        neg eax
        sbb eax, eax
        and eax, 5
        ret
    }
}
