// Divide two fixed-point products through the shared integer conversion path.
__declspec(naked) int FUN_004f5f10()
{
    __asm {
        fild dword ptr [esp+4]
        fimul dword ptr [esp+8]
        fidiv dword ptr [esp+0xc]
        _emit 0xe9
        _emit 0xd3
        _emit 0xa5
        _emit 0x00
        _emit 0x00
    }
}
