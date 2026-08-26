__declspec(naked) int FUN_004e8f00()
{
    __asm {
        mov eax, dword ptr [esp+8]
        push 0
        push eax
        _emit 0xe8
        _emit 0xf4
        _emit 0xe9
        _emit 0xff
        _emit 0xff
        push eax
        _emit 0xe8
        _emit 0xae
        _emit 0xec
        _emit 0xff
        _emit 0xff
        add esp, 0x0c
        xor eax, eax
        ret
    }
}
