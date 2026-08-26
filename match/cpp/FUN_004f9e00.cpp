// Configure the standard 8-bit renderer parser profile.
__declspec(naked) void FUN_004f9e00()
{
    __asm {
        mov eax, dword ptr [esp+0x10]
        mov ecx, dword ptr [esp+0xc]
        mov edx, dword ptr [esp+8]
        push eax
        mov eax, dword ptr [esp+8]
        push ecx
        push 0
        push 8
        push 0xf
        push 8
        push edx
        push eax
        _emit 0xe8
        _emit 0x0f
        _emit 0x00
        _emit 0x00
        _emit 0x00
        add esp, 0x20
        ret
    }
}
