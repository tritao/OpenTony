// Normalize axis ordering and record the resulting transform extents.
__declspec(naked) void FUN_004f4050()
{
    __asm {
        mov eax, dword ptr [esp+8]
        mov ecx, dword ptr [esp+4]
        mov edx, dword ptr [esp+0xc]
        push ebx
        push ebp
        push esi
        mov esi, dword ptr [eax]
        push edi
        mov edi, dword ptr [ecx]
        cmp esi, edi
        jnl short axis_x_done
        mov dword ptr [eax], edi
        mov dword ptr [ecx], esi
        xor dword ptr [edx], 1
    axis_x_done:
        mov ebp, dword ptr [eax]
        mov esi, dword ptr [ecx]
        mov edi, dword ptr [ecx+4]
        sub ebp, esi
        mov esi, dword ptr [eax+4]
        cmp esi, edi
        jnl short axis_y_done
        mov dword ptr [eax+4], edi
        mov dword ptr [ecx+4], esi
        xor dword ptr [edx], 2
    axis_y_done:
        mov esi, dword ptr [eax+4]
        mov edi, dword ptr [ecx+4]
        mov ebx, dword ptr [ecx+8]
        sub esi, edi
        mov edi, dword ptr [eax+8]
        cmp edi, ebx
        jnl short axis_z_done
        mov dword ptr [eax+8], ebx
        mov dword ptr [ecx+8], edi
        xor dword ptr [edx], 4
    axis_z_done:
        mov eax, dword ptr [eax+8]
        mov edx, dword ptr [ecx+8]
        _emit 0x66
        _emit 0x89
        _emit 0x35
        _emit 0x18
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        pop edi
        sub eax, edx
        _emit 0x66
        _emit 0x89
        _emit 0x2d
        _emit 0x10
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        pop esi
        pop ebp
        _emit 0x66
        _emit 0xa3
        _emit 0x20
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        pop ebx
        ret
    }
}
