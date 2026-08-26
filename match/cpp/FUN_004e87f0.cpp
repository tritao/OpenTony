// Copy viewport extents into the render state, replacing zero extents by one.
__declspec(naked) void FUN_004e87f0(void *render_state, short *extents)
{
    __asm {
        mov eax, dword ptr [esp+4]
        mov ecx, dword ptr [esp+8]
        mov dword ptr [eax+4], 0xe3000000
        mov dx, word ptr [ecx]
        mov word ptr [eax+8], dx
        mov dx, word ptr [ecx+2]
        mov word ptr [eax+0xa], dx
        mov dx, word ptr [ecx+4]
        mov word ptr [eax+0xc], dx
        mov cx, word ptr [ecx+6]
        mov word ptr [eax+0xe], cx
        mov ecx, 1
        test dx, dx
        jne short have_height
        mov word ptr [eax+0xc], cx
    have_height:
        cmp word ptr [eax+0xe], 0
        jne short done
        mov word ptr [eax+0xe], cx
    done:
        ret
    }
}
