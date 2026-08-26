// Forward two scalar arguments to the shared transform operation.
__declspec(naked) void FUN_004f5390()
{
    __asm {
        mov eax, dword ptr [esp+8]
        mov ecx, dword ptr [esp+4]
        push eax
        push ecx
        _emit 0xe8
        _emit 0x41
        _emit 0x2d
        _emit 0xff
        _emit 0xff
        add esp, 8
        ret
    }
}
