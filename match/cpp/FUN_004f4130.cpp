// Build integer and sub-voxel bounds from a fixed-point box, honoring axis flips.
__declspec(naked) void FUN_004f4130()
{
    __asm {
        mov eax, dword ptr [esp+4]
        mov ecx, dword ptr [esp+0x30]
        push ebx
        push ebp
        mov edx, dword ptr [eax+0xc]
        push esi
        mov esi, dword ptr [eax+0x10]
        push edi
        mov edi, dword ptr [eax+0x14]
        mov eax, dword ptr [esp+0x30]
        mov ebx, edx
        mov ebp, esi
        sar ebx, 0x10
        add ebx, eax
        mov dword ptr [ecx], ebx
        mov ebx, dword ptr [esp+0x34]
        sar ebp, 0x10
        add ebp, ebx
        mov dword ptr [ecx+4], ebp
        mov ebp, edi
        sar ebp, 0x10
        add ebp, dword ptr [esp+0x38]
        shl edx, 0x10
        sar edx, 0x10
        shl esi, 0x10
        shl edi, 0x10
        mov dword ptr [ecx+8], ebp
        mov ebp, dword ptr [esp+0x38]
        add edx, eax
        mov eax, dword ptr [esp+0x44]
        sar esi, 0x10
        sar edi, 0x10
        add esi, ebx
        add edi, ebp
        mov dword ptr [eax], edx
        mov dword ptr [eax+4], esi
        mov dword ptr [eax+8], edi
        mov ebx, dword ptr [ecx]
        mov edi, dword ptr [ecx+4]
        mov esi, dword ptr [ecx+8]
        mov edx, -2
        add ebx, edx
        add edi, edx
        add esi, edx
        mov dword ptr [ecx], ebx
        mov dword ptr [ecx+4], edi
        mov dword ptr [ecx+8], esi
        mov ebx, dword ptr [eax+4]
        mov ebp, dword ptr [eax]
        mov esi, dword ptr [eax+8]
        mov edx, 2
        add ebx, edx
        add ebp, edx
        mov dword ptr [eax+4], ebx
        mov bl, byte ptr [esp+0x3c]
        add esi, edx
        mov dword ptr [eax], ebp
        test bl, 1
        mov edi, ebp
        mov dword ptr [eax+8], esi
        jz short no_x_flip
        mov edx, dword ptr [esp+0x18]
        mov esi, dword ptr [esp+0x24]
        mov ebp, dword ptr [ecx]
        add edx, esi
        mov esi, edx
        sub esi, ebp
        sub edx, edi
        mov dword ptr [ecx], edx
        mov dword ptr [eax], esi
    no_x_flip:
        test bl, 2
        jz short no_y_flip
        mov edx, dword ptr [esp+0x1c]
        mov esi, dword ptr [esp+0x28]
        mov ebp, dword ptr [ecx+4]
        mov edi, dword ptr [eax+4]
        add edx, esi
        mov esi, edx
        sub esi, ebp
        sub edx, edi
        mov dword ptr [ecx+4], edx
        mov dword ptr [eax+4], esi
    no_y_flip:
        test bl, 4
        jz short finish
        mov edx, dword ptr [esp+0x20]
        mov esi, dword ptr [esp+0x2c]
        mov ebx, dword ptr [ecx+8]
        mov edi, dword ptr [eax+8]
        add edx, esi
        mov esi, edx
        sub esi, ebx
        sub edx, edi
        mov dword ptr [ecx+8], edx
        mov dword ptr [eax+8], esi
    finish:
        pop edi
        pop esi
        pop ebp
        pop ebx
        ret
    }
}
