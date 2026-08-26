// Pump pending Windows messages while filtering keyboard character messages.
__declspec(naked) int Game_FrameTick()
{
    __asm {
        sub esp, 0x1c
        _emit 0xa1
        _emit 0x74
        _emit 0x83
        _emit 0x9d
        _emit 0x02
        _emit 0x8d
        _emit 0x4c
        _emit 0x24
        _emit 0x00
        push ebx
        push ebp
        _emit 0x8b
        _emit 0x2d
        _emit 0xa8
        _emit 0x82
        _emit 0x51
        _emit 0x00
        push esi
        push edi
        push 0
        push 0
        push 0
        push eax
        push ecx
        call ebp
        test eax, eax
        jz short no_message
        _emit 0x8b
        _emit 0x35
        _emit 0xa0
        _emit 0x82
        _emit 0x51
        _emit 0x00
        _emit 0x8b
        _emit 0x3d
        _emit 0xac
        _emit 0x82
        _emit 0x51
        _emit 0x00
        _emit 0x8b
        _emit 0x1d
        _emit 0xa4
        _emit 0x82
        _emit 0x51
        _emit 0x00
    message_loop:
        _emit 0x8b
        _emit 0x15
        _emit 0x74
        _emit 0x83
        _emit 0x9d
        _emit 0x02
        push 0
        push 0
        lea eax, [esp+0x18]
        push edx
        push eax
        call esi
        test eax, eax
        jz short quit_message
        mov eax, dword ptr [esp+0x14]
        cmp eax, 0x104
        jz short skip_dispatch
        cmp eax, 0x105
        jz short skip_dispatch
        lea ecx, [esp+0x10]
        push ecx
        call edi
        lea edx, [esp+0x10]
        push edx
        call ebx
    skip_dispatch:
        _emit 0xa1
        _emit 0x74
        _emit 0x83
        _emit 0x9d
        _emit 0x02
        push 0
        push 0
        push 0
        lea ecx, [esp+0x1c]
        push eax
        push ecx
        call ebp
        test eax, eax
        jnz short message_loop
    no_message:
        pop edi
        pop esi
        pop ebp
        mov eax, 0x75
        pop ebx
        add esp, 0x1c
        ret
    quit_message:
        mov eax, dword ptr [esp+0x18]
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x1c
        ret
    }
}
