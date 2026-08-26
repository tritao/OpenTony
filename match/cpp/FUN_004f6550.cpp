// Sleep for the requested number of milliseconds through the imported Win32 API.
__declspec(naked) void FUN_004f6550()
{
    __asm {
        mov eax, dword ptr [esp+4]
        push eax
        _emit 0xff
        _emit 0x15
        _emit 0x7c
        _emit 0x80
        _emit 0x51
        _emit 0x00
        ret
    }
}
