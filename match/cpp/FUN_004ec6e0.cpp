__declspec(naked) void* FUN_004ec6e0()
{
    __asm {
        push esi
        mov esi, ecx
        mov dword ptr [esi], 0x00519a20
        mov dword ptr [esi+0x28], 0
        mov byte ptr [esi+0x50], 0
        _emit 0xe8
        _emit 0x67
        _emit 0x00
        _emit 0x00
        _emit 0x00
        push 0x1e0
        push 0x280
        push 0
        push 0
        mov ecx, esi
        _emit 0xe8
        _emit 0x22
        _emit 0x02
        _emit 0x00
        _emit 0x00
        mov dword ptr [esi+0x0c], 0x100
        mov eax, esi
        pop esi
        ret
    }
}
