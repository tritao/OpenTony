// Compute a fixed-point dot product and use the shared integer conversion.
__declspec(naked) int FUN_004f5f90()
{
    __asm {
        mov eax, dword ptr [esp+4]
        mov ecx, dword ptr [esp+8]
        fild dword ptr [eax+8]
        fimul dword ptr [ecx+8]
        fild dword ptr [eax+4]
        fimul dword ptr [ecx+4]
        _emit 0xde
        _emit 0xc1
        fild dword ptr [eax]
        fimul dword ptr [ecx]
        _emit 0xde
        _emit 0xc1
        _emit 0xdc
        _emit 0x0d
        _emit 0x08
        _emit 0x99
        _emit 0x51
        _emit 0x00
        _emit 0xe9
        _emit 0x3d
        _emit 0xa5
        _emit 0x00
        _emit 0x00
    }
}
