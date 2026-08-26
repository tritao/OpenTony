// Initialize the renderer's z-buffer description and register its callback.
__declspec(naked) void FUN_004f7a70()
{
    __asm {
        sub esp, 0x7c
        push esi
        push edi
        mov ecx, 0x1f
        xor eax, eax
        lea edi, [esp+8]
        push 0x4f79e0
        rep stosd
        _emit 0xa1
        _emit 0xa8
        _emit 0xa3
        _emit 0x9d
        _emit 0x02
        lea edx, [esp+0xc]
        push 0x29d8478
        mov dword ptr [esp+0x10], 0x7c
        mov dword ptr [esp+0x64], 0x10
        mov dword ptr [esp+0x78], 0x2000
        mov ecx, dword ptr [eax]
        push edx
        push 0
        push eax
        call dword ptr [ecx+0x20]
        mov esi, eax
        test esi, esi
        jz short done
        push 0x718
        push 0x55bf38
        push esi
        _emit 0xe8
        _emit 0x94
        _emit 0x80
        _emit 0xfd
        _emit 0xff
        push 1
        _emit 0xe8
        _emit 0xad
        _emit 0x94
        _emit 0xfd
        _emit 0xff
        add esp, 0x10
        push esi
        _emit 0xe8
        _emit 0x5d
        _emit 0x99
        _emit 0x00
        _emit 0x00
    done:
        pop edi
        pop esi
        add esp, 0x7c
        ret
    }
}
