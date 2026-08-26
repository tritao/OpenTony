// Interpolate a state record with independent vector and transform masks.
__declspec(naked) void FUN_004f21a0()
{
    __asm {
        mov eax, dword ptr [esp+8]
        mov ecx, dword ptr [esp+0xc]
        fld dword ptr [esp+0x10]
        fsub dword ptr [eax+4]
        fld dword ptr [ecx+4]
        fsub dword ptr [eax+4]
        mov edx, dword ptr [esp+4]
        push ebx
        _emit 0xde
        _emit 0xf9
        mov bl, byte ptr [esp+0x18]
        test bl, bl
        fld dword ptr [esp+0x14]
        fstp dword ptr [edx+4]
        fld dword ptr [ecx]
        fsub dword ptr [eax]
        _emit 0xd8
        _emit 0xc9
        fadd dword ptr [eax]
        fstp dword ptr [edx]
        jz short skip_vectors
        fld dword ptr [ecx+0xc]
        fsub dword ptr [eax+0xc]
        _emit 0xd8
        _emit 0xc9
        fadd dword ptr [eax+0xc]
        fstp dword ptr [edx+0xc]
        fld dword ptr [ecx+0x14]
        fsub dword ptr [eax+0x14]
        _emit 0xd8
        _emit 0xc9
        fadd dword ptr [eax+0x14]
        fstp dword ptr [edx+0x14]
        fld dword ptr [ecx+0x18]
        fsub dword ptr [eax+0x18]
        _emit 0xd8
        _emit 0xc9
        fadd dword ptr [eax+0x18]
        fstp dword ptr [edx+0x18]
    skip_vectors:
        mov bl, byte ptr [esp+0x1c]
        test bl, bl
        pop ebx
        jz short done_pop
        fld dword ptr [ecx+0x1c]
        fsub dword ptr [eax+0x1c]
        _emit 0xd8
        _emit 0xc9
        fadd dword ptr [eax+0x1c]
        fstp dword ptr [edx+0x1c]
        fld dword ptr [ecx+0x24]
        fsub dword ptr [eax+0x24]
        _emit 0xd8
        _emit 0xc9
        fadd dword ptr [eax+0x24]
        fstp dword ptr [edx+0x24]
        fld dword ptr [ecx+0x20]
        fsub dword ptr [eax+0x20]
        _emit 0xd8
        _emit 0xc9
        fadd dword ptr [eax+0x20]
        fstp dword ptr [edx+0x20]
    done_pop:
        _emit 0xdd
        _emit 0xd8
        ret
    }
}
