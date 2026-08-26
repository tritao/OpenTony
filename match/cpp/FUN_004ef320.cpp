__declspec(naked) void FUN_004ef320()
{
    __asm {
        mov dl, byte ptr [esp+0xc]
        test dl, 0x48
        je short normal
        mov eax, dword ptr [esp+4]
        mov ecx, dword ptr [esp+8]
        push esi
        mov esi, dword ptr [eax+0xc]
        mov dword ptr [ecx+0x40], esi
        mov dword ptr [eax+0xc], ecx
        test dl, 8
        pop esi
        je short done
        or byte ptr [eax], 1
        ret
    normal:
        mov eax, dword ptr [esp+4]
        mov ecx, dword ptr [esp+8]
        mov edx, dword ptr [eax+8]
        mov dword ptr [ecx+0x40], edx
        mov dword ptr [eax+8], ecx
        mov dx, word ptr [ecx+0x4a]
        add word ptr [eax+2], dx
        xor eax, eax
        mov ax, word ptr [ecx+0x4a]
        _emit 0x8b
        _emit 0x0d
        _emit 0xb4
        _emit 0x68
        _emit 0x9d
        _emit 0x02
        sub ecx, eax
        _emit 0x89
        _emit 0x0d
        _emit 0xb4
        _emit 0x68
        _emit 0x9d
        _emit 0x02
    done:
        ret
    }
}
