__declspec(naked) void* FUN_004ed490()
{
    __asm {
        push esi
        mov esi, ecx
        _emit 0xe8
        _emit 0x18
        _emit 0x00
        _emit 0x00
        _emit 0x00
        test byte ptr [esp+8], 1
        je short done
        push esi
        _emit 0xe8
        _emit 0x4e
        _emit 0x2e
        _emit 0x01
        _emit 0x00
        add esp, 4
    done:
        mov eax, esi
        pop esi
        ret 4
    }
}
