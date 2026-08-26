// Transform two packed 3D records through the collision orientation helpers.
__declspec(naked) void FUN_004f5a70()
{
    __asm {
        sub esp, 0x18
        mov eax, dword ptr [esp+0x20]
        push ebx
        push ebp
        push esi
        mov cx, word ptr [eax]
        push edi
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
        movsx edx, word ptr [eax+0x12]
        _emit 0x89
        _emit 0x15
        _emit 0xa0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        movsx ecx, word ptr [eax+0x14]
        _emit 0x89
        _emit 0x0d
        _emit 0xa4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        movsx edx, word ptr [eax+0x16]
        mov eax, dword ptr [esp+0x34]
        _emit 0x89
        _emit 0x15
        _emit 0xa8
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov bp, word ptr [eax+0xa]
        mov cx, word ptr [eax]
        mov word ptr [esp+0x1c], bp
        mov bp, word ptr [eax+0xc]
        mov word ptr [esp+0x10], bp
        mov bp, word ptr [eax+0xe]
        mov dx, word ptr [eax+2]
        mov si, word ptr [eax+4]
        mov di, word ptr [eax+6]
        mov bx, word ptr [eax+8]
        mov word ptr [esp+0x12], bp
        mov bp, word ptr [eax+0x10]
        mov word ptr [esp+0x14], bp
        mov bp, word ptr [eax+0x12]
        mov word ptr [esp+0x20], bp
        mov bp, word ptr [eax+0x14]
        mov ax, word ptr [eax+0x16]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x58
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [esp+0x1c]
        mov word ptr [esp+0x24], ax
        mov ax, word ptr [esp+0x12]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x5a
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [esp+0x10]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x64
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [esp+0x14]
        _emit 0x66
        _emit 0x89
        _emit 0x35
        _emit 0x5c
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x3d
        _emit 0x60
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x1d
        _emit 0x62
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x68
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0xa3
        _emit 0x6a
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x6c
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0xe8
        _emit 0xfd
        _emit 0xdd
        _emit 0xfe
        _emit 0xff
        _emit 0x66
        _emit 0x8b
        _emit 0x35
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x8b
        _emit 0x3d
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x8b
        _emit 0x1d
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0xe8
        _emit 0xe3
        _emit 0xdf
        _emit 0xfe
        _emit 0xff
        _emit 0x66
        _emit 0x8b
        _emit 0x15
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0xa1
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x8b
        _emit 0x0d
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov word ptr [esp+0x18], dx
        mov word ptr [esp+0x1a], ax
        mov word ptr [esp+0x1c], cx
        _emit 0xe8
        _emit 0x5b
        _emit 0xe0
        _emit 0xfe
        _emit 0xff
        _emit 0x66
        _emit 0x8b
        _emit 0x15
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0xa1
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x8b
        _emit 0x0d
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov word ptr [esp+0x10], dx
        mov dx, word ptr [esp+0x20]
        mov word ptr [esp+0x12], ax
        mov word ptr [esp+0x14], cx
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov ax, word ptr [esp+0x24]
        _emit 0x66
        _emit 0x89
        _emit 0x2d
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0xa3
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0xe8
        _emit 0xa5
        _emit 0xd5
        _emit 0xfe
        _emit 0xff
        mov eax, dword ptr [esp+0x2c]
        _emit 0x66
        _emit 0x8b
        _emit 0x0d
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x8b
        _emit 0x15
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x8b
        _emit 0x2d
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov word ptr [eax], si
        mov si, word ptr [esp+0x18]
        mov word ptr [eax+6], si
        mov si, word ptr [esp+0x1a]
        mov word ptr [eax+8], si
        mov si, word ptr [esp+0x1c]
        mov word ptr [eax+0xa], si
        mov si, word ptr [esp+0x10]
        mov word ptr [eax+0xc], si
        mov si, word ptr [esp+0x12]
        mov word ptr [eax+0xe], si
        mov si, word ptr [esp+0x14]
        mov word ptr [eax+2], di
        mov word ptr [eax+0x10], si
        pop edi
        mov word ptr [eax+0x16], bp
        pop esi
        mov word ptr [eax+4], bx
        pop ebp
        mov word ptr [eax+0x12], cx
        mov word ptr [eax+0x14], dx
        pop ebx
        add esp, 0x18
        ret
    }
}
