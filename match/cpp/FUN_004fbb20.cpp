// Forward two parser arguments to the shared input conversion helper.
__declspec(naked) void FUN_004fbb20()
{
    __asm {
        mov eax, dword ptr [esp+0xc]
        mov ecx, dword ptr [esp+8]
        push eax
        push ecx
        _emit 0xe8
        _emit 0x96
        _emit 0x75
        _emit 0x00
        _emit 0x00
        add esp, 8
        ret
    }
}
