// Return the two persisted profile coordinates.
__declspec(naked) void FUN_004f6d00()
{
    __asm {
        mov eax, dword ptr [esp+4]
        _emit 0x8b
        _emit 0x0d
        _emit 0x94
        _emit 0xa3
        _emit 0x9d
        _emit 0x02
        mov edx, dword ptr [esp+8]
        mov dword ptr [eax], ecx
        _emit 0xa1
        _emit 0x98
        _emit 0xa3
        _emit 0x9d
        _emit 0x02
        mov dword ptr [edx], eax
        ret
    }
}
