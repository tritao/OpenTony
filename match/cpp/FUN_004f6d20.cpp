// Toggle the renderer and reset device state around a level transition.
__declspec(naked) void FUN_004f6d20()
{
    __asm {
        sub esp, 8
        _emit 0xa1
        _emit 0xa0
        _emit 0xa3
        _emit 0x9d
        _emit 0x02
        push ebx
        xor bl, bl
        push esi
        test eax, eax
        setnz cl
        mov byte ptr [esp+0xc], cl
        mov ecx, dword ptr [esp+0x14]
        cmp ecx, 4
        mov byte ptr [esp+8], bl
        jnz short not_four
        test eax, eax
        jz short publish_state
        push 0
        _emit 0xe8
        _emit 0x33
        _emit 0xa2
        _emit 0xfd
        _emit 0xff
        add esp, 4
        xor eax, eax
        mov bl, 1
        _emit 0xa3
        _emit 0xa0
        _emit 0xa3
        _emit 0x9d
        _emit 0x02
        mov byte ptr [esp+8], bl
    publish_state:
        _emit 0xc6
        _emit 0x05
        _emit 0x24
        _emit 0x9f
        _emit 0x54
        _emit 0x00
        _emit 0x01
    render_call:
        push eax
        mov eax, dword ptr [esp+0x10]
        and eax, 0xff
        push eax
        push 0x55bf58
        _emit 0xe8
        _emit 0xd7
        _emit 0x8a
        _emit 0xfd
        _emit 0xff
        add esp, 0xc
        test bl, bl
        jz short finish
        mov ecx, dword ptr [esp+8]
        push ecx
        _emit 0xe8
        _emit 0x86
        _emit 0x03
        _emit 0x00
        _emit 0x00
        add esp, 4
    finish:
        pop esi
        pop ebx
        add esp, 8
        ret

    not_four:
        cmp ecx, 8
        jnz short render_call
        test eax, eax
        jnz renderer_already_active
        _emit 0xa1
        _emit 0xb0
        _emit 0xa3
        _emit 0x9d
        _emit 0x02
        _emit 0x8b
        _emit 0x0d
        _emit 0xb4
        _emit 0xa3
        _emit 0x9d
        _emit 0x02
        push ecx
        push 0
        mov edx, dword ptr [eax]
        push eax
        call dword ptr [edx+0x20]
        mov esi, eax
        test esi, esi
        jz short no_first_device
        push 0x52a
        push 0x55bf38
        push esi
        _emit 0xe8
        _emit 0x96
        _emit 0x8d
        _emit 0xfd
        _emit 0xff
        push 1
        _emit 0xe8
        _emit 0xaf
        _emit 0xa1
        _emit 0xfd
        _emit 0xff
        add esp, 0x10
        push esi
        _emit 0xe8
        _emit 0x5f
        _emit 0xa6
        _emit 0x00
        _emit 0x00
    no_first_device:
        _emit 0xa1
        _emit 0xb4
        _emit 0xa3
        _emit 0x9d
        _emit 0x02
        push eax
        mov edx, dword ptr [eax]
        call dword ptr [edx+8]
        mov esi, eax
        test esi, esi
        jz short no_second_device
        push 0x52d
        push 0x55bf38
        push esi
        _emit 0xe8
        _emit 0x65
        _emit 0x8d
        _emit 0xfd
        _emit 0xff
        push 1
        _emit 0xe8
        _emit 0x7e
        _emit 0xa1
        _emit 0xfd
        _emit 0xff
        add esp, 0x10
        push esi
        _emit 0xe8
        _emit 0x2e
        _emit 0xa6
        _emit 0x00
        _emit 0x00
    no_second_device:
        push 0
        _emit 0xc7
        _emit 0x05
        _emit 0xb4
        _emit 0xa3
        _emit 0x9d
        _emit 0x02
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0xe8
        _emit 0x64
        _emit 0xa1
        _emit 0xfd
        _emit 0xff
        add esp, 4
        mov bl, 1
        mov byte ptr [esp+8], 0
    renderer_already_active:
        mov ecx, dword ptr [esp+0x18]
        test ecx, ecx
        lea eax, [ecx+1]
        _emit 0xa3
        _emit 0xa0
        _emit 0xa3
        _emit 0x9d
        _emit 0x02
        jnz short publish_state
        _emit 0x8b
        _emit 0x0d
        _emit 0x98
        _emit 0xa8
        _emit 0x56
        _emit 0x00
        cmp ecx, 9
        jng short clear_renderer
        cmp ecx, 0xd
        jl publish_state
    clear_renderer:
        _emit 0xc6
        _emit 0x05
        _emit 0x24
        _emit 0x9f
        _emit 0x54
        _emit 0x00
        _emit 0x00
        jmp render_call
    }
}
