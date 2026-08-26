// Convert a packed angle to the original fixed-point cosine result.
__declspec(naked) int FUN_004f3980()
{
    __asm {
        mov eax, dword ptr [esp+4]
        and eax, 0xfff
        mov dword ptr [esp+4], eax
        fild dword ptr [esp+4]
        _emit 0xd8
        _emit 0x0d
        _emit 0x10
        _emit 0x89
        _emit 0x51
        _emit 0x00
        _emit 0xd8
        _emit 0x0d
        _emit 0x08
        _emit 0x9a
        _emit 0x51
        _emit 0x00
        fcos
        _emit 0xd8
        _emit 0x0d
        _emit 0xbc
        _emit 0x99
        _emit 0x51
        _emit 0x00
        _emit 0xe9
        _emit 0x4a
        _emit 0xcb
        _emit 0x00
        _emit 0x00
    }
}
