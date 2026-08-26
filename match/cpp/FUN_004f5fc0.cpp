// Scale one fixed-point component and tail-dispatch to integer conversion.
__declspec(naked) int FUN_004f5fc0()
{
    __asm {
        fild dword ptr [esp+4]
        fimul dword ptr [esp+8]
        _emit 0xdc
        _emit 0x0d
        _emit 0x08
        _emit 0x99
        _emit 0x51
        _emit 0x00
        _emit 0xe9
        _emit 0x21
        _emit 0xa5
        _emit 0x00
        _emit 0x00
    }
}
