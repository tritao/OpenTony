// Apply a fixed-point transform using the engine's shared trigonometric state.
__declspec(naked) int *FUN_004e85a0()
{
    __asm {
        sub esp, 0x10
        mov eax, dword ptr [esp+0x14]
        push esi
        mov cx, word ptr [eax]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x10
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [eax+2]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x12
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [eax+4]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x14
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [eax+6]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x16
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [eax+8]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x18
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [eax+0xa]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x1a
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [eax+0xc]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x1c
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [eax+0xe]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x1e
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [eax+0x10]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x20
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov ecx, dword ptr [esp+0x1c]
        movsx edx, word ptr [eax+0xa]
        fild dword ptr [ecx+8]
        fild dword ptr [ecx+4]
        fild dword ptr [ecx]
        movsx ecx, word ptr [eax+6]
        mov dword ptr [esp+0x18], edx
        fild dword ptr [esp+0x18]
        movsx edx, word ptr [eax+8]
        _emit 0xd8
        _emit 0xcb
        mov dword ptr [esp+0x18], ecx
        fild dword ptr [esp+0x18]
        mov dword ptr [esp+0x18], edx
        _emit 0xd8
        _emit 0xca
        movsx ecx, word ptr [eax+0xc]
        _emit 0xde
        _emit 0xc1
        fild dword ptr [esp+0x18]
        movsx edx, word ptr [eax+0xe]
        _emit 0xd8
        _emit 0xcb
        mov dword ptr [esp+0x18], ecx
        movsx ecx, word ptr [eax+0x10]
        _emit 0xde
        _emit 0xc1
        _emit 0xdc
        _emit 0x0d
        _emit 0x08
        _emit 0x99
        _emit 0x51
        _emit 0x00
        _emit 0xdd
        _emit 0x5c
        _emit 0x24
        _emit 0x04
        fild dword ptr [esp+0x18]
        mov dword ptr [esp+0x18], edx
        movsx edx, word ptr [eax]
        _emit 0xd8
        _emit 0xc9
        fild dword ptr [esp+0x18]
        mov dword ptr [esp+0x18], ecx
        _emit 0xd8
        _emit 0xcb
        movsx ecx, word ptr [eax+4]
        _emit 0xde
        _emit 0xc1
        fild dword ptr [esp+0x18]
        mov dword ptr [esp+0x18], edx
        movsx edx, word ptr [eax+2]
        _emit 0xd8
        _emit 0xcc
        _emit 0xde
        _emit 0xc1
        _emit 0xdc
        _emit 0x0d
        _emit 0x08
        _emit 0x99
        _emit 0x51
        _emit 0x00
        _emit 0xdd
        _emit 0x5c
        _emit 0x24
        _emit 0x0c
        fild dword ptr [esp+0x18]
        mov dword ptr [esp+0x18], ecx
        _emit 0xd8
        _emit 0xc9
        fild dword ptr [esp+0x18]
        mov dword ptr [esp+0x18], edx
        _emit 0xd8
        _emit 0xcc
        _emit 0xde
        _emit 0xc1
        fild dword ptr [esp+0x18]
        _emit 0xd8
        _emit 0xcb
        _emit 0xde
        _emit 0xc1
        _emit 0xdc
        _emit 0x0d
        _emit 0x08
        _emit 0x99
        _emit 0x51
        _emit 0x00
        _emit 0xe8
        _emit 0x36
        _emit 0x7e
        _emit 0x01
        _emit 0x00
        mov esi, dword ptr [esp+0x20]
        _emit 0xdd
        _emit 0xd8
        _emit 0xdd
        _emit 0xd8
        _emit 0xdd
        _emit 0xd8
        fld qword ptr [esp+4]
        mov dword ptr [esi], eax
        _emit 0xe8
        _emit 0x21
        _emit 0x7e
        _emit 0x01
        _emit 0x00
        fld qword ptr [esp+0xc]
        mov dword ptr [esi+4], eax
        _emit 0xe8
        _emit 0x15
        _emit 0x7e
        _emit 0x01
        _emit 0x00
        mov dword ptr [esi+8], eax
        mov eax, esi
        pop esi
        add esp, 0x10
        ret
    }
}
