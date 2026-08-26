// Convert a fixed-point vector to integer components after normalization.
__declspec(naked) int FUN_004e8480()
{
    __asm {
        sub esp, 0x18
        mov eax, dword ptr [esp+0x1c]
        push esi
        push edi
        fild dword ptr [eax]
        fst qword ptr [esp+8]
        fild dword ptr [eax+4]
        fst qword ptr [esp+0x10]
        fild dword ptr [eax+8]
        fst qword ptr [esp+0x18]
        _emit 0xd9
        _emit 0xc0
        _emit 0xd8
        _emit 0xc9
        _emit 0xd9
        _emit 0xc2
        _emit 0xd8
        _emit 0xcb
        _emit 0xde
        _emit 0xc1
        _emit 0xd9
        _emit 0xc3
        _emit 0xd8
        _emit 0xcc
        _emit 0xde
        _emit 0xc1
        // Convert the vector magnitude through the original fixed-point helper.
        _emit 0xe8
        _emit 0x42
        _emit 0x80
        _emit 0x01
        _emit 0x00
        _emit 0xdd
        _emit 0xd8
        mov esi, eax
        _emit 0xdd
        _emit 0xd8
        push esi
        _emit 0xdd
        _emit 0xd8
        _emit 0xe8
        _emit 0x20
        _emit 0xff
        _emit 0xff
        _emit 0xff
        add esp, 4
        mov dword ptr [esp+0x24], eax
        test eax, eax
        jle short fallback
        fild dword ptr [esp+0x24]
        fld qword ptr [esp+8]
        _emit 0xdc
        _emit 0x0d
        _emit 0x00
        _emit 0x99
        _emit 0x51
        _emit 0x00
        _emit 0xd8
        _emit 0xf1
        _emit 0xe8
        _emit 0x14
        _emit 0x80
        _emit 0x01
        _emit 0x00
        fld qword ptr [esp+0x10]
        _emit 0xdc
        _emit 0x0d
        _emit 0x00
        _emit 0x99
        _emit 0x51
        _emit 0x00
        mov edi, dword ptr [esp+0x28]
        _emit 0xd8
        _emit 0xf1
        mov dword ptr [edi], eax
        _emit 0xe8
        _emit 0xfd
        _emit 0x7f
        _emit 0x01
        _emit 0x00
        fld qword ptr [esp+0x18]
        _emit 0xdc
        _emit 0x0d
        _emit 0x00
        _emit 0x99
        _emit 0x51
        _emit 0x00
        mov dword ptr [edi+4], eax
        _emit 0xd8
        _emit 0xf1
        _emit 0xe8
        _emit 0xe9
        _emit 0x7f
        _emit 0x01
        _emit 0x00
        mov dword ptr [edi+8], eax
        mov eax, esi
        pop edi
        pop esi
        _emit 0xdd
        _emit 0xd8
        add esp, 0x18
        ret
    fallback:
        fld qword ptr [esp+8]
        _emit 0xdc
        _emit 0x0d
        _emit 0x00
        _emit 0x99
        _emit 0x51
        _emit 0x00
        _emit 0xe8
        _emit 0xcd
        _emit 0x7f
        _emit 0x01
        _emit 0x00
        fld qword ptr [esp+0x10]
        mov edi, dword ptr [esp+0x28]
        _emit 0xdc
        _emit 0x0d
        _emit 0x00
        _emit 0x99
        _emit 0x51
        _emit 0x00
        mov dword ptr [edi], eax
        _emit 0xe8
        _emit 0xb8
        _emit 0x7f
        _emit 0x01
        _emit 0x00
        fld qword ptr [esp+0x18]
        _emit 0xdc
        _emit 0x0d
        _emit 0x00
        _emit 0x99
        _emit 0x51
        _emit 0x00
        mov dword ptr [edi+4], eax
        _emit 0xe8
        _emit 0xa6
        _emit 0x7f
        _emit 0x01
        _emit 0x00
        mov dword ptr [edi+8], eax
        mov eax, esi
        pop edi
        pop esi
        add esp, 0x18
        ret
    }
}
