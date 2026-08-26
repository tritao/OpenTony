// Interpolate flagged float fields from one effect state into another.
__declspec(naked) void FUN_004ef820()
{
    __asm {
        mov ecx, dword ptr [esp+8]
        fld dword ptr [ecx]
        fcomp dword ptr [esp+0xc]
        fnstsw ax
        test ah, 0x40
        jne short done
        mov eax, dword ptr [esp+4]
        mov edx, dword ptr [esp+0xc]
        fld dword ptr [esp+0xc]
        fsub dword ptr [eax]
        fld dword ptr [ecx]
        fsub dword ptr [eax]
        mov dword ptr [ecx], edx
        mov dl, byte ptr [esp+0x10]
        test dl, 2
        _emit 0xde
        _emit 0xf9
        je short skip_vectors
        fld dword ptr [ecx+0x14]
        fsub dword ptr [eax+0x14]
        test dl, 4
        _emit 0xd8
        _emit 0xc9
        fadd dword ptr [eax+0x14]
        fstp dword ptr [ecx+0x14]
        fld dword ptr [ecx+0x18]
        fsub dword ptr [eax+0x18]
        _emit 0xd8
        _emit 0xc9
        fadd dword ptr [eax+0x18]
        fstp dword ptr [ecx+0x18]
        je short skip_vectors
        fld dword ptr [ecx+0xc]
        fsub dword ptr [eax+0xc]
        _emit 0xd8
        _emit 0xc9
        fadd dword ptr [eax+0xc]
        fstp dword ptr [ecx+0xc]
    skip_vectors:
        test dl, 1
        je short done_pop
        fld dword ptr [ecx+0x1c]
        fsub dword ptr [eax+0x1c]
        _emit 0xd8
        _emit 0xc9
        fadd dword ptr [eax+0x1c]
        fstp dword ptr [ecx+0x1c]
        fld dword ptr [ecx+0x24]
        fsub dword ptr [eax+0x24]
        _emit 0xd8
        _emit 0xc9
        fadd dword ptr [eax+0x24]
        fstp dword ptr [ecx+0x24]
        fld dword ptr [ecx+0x20]
        fsub dword ptr [eax+0x20]
        _emit 0xd8
        _emit 0xc9
        fadd dword ptr [eax+0x20]
        fstp dword ptr [ecx+0x20]
    done_pop:
        _emit 0xdd
        _emit 0xd8
    done:
        ret
    }
}
