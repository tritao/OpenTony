// Build a transformed collision record and derive its translated extents.
__declspec(naked) void FUN_004f5ca0()
{
    __asm {
        sub esp, 0x18
        push ebx
        push ebp
        push esi
        push edi
        mov edi, dword ptr [esp+0x30]
        mov ebx, dword ptr [esp+0x34]
        mov ax, word ptr [edi]
        _emit 0x66
        _emit 0xa3
        _emit 0x10
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [edi+2]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x12
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [edi+4]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x14
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov ax, word ptr [edi+6]
        _emit 0x66
        _emit 0xa3
        _emit 0x16
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [edi+8]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x18
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [edi+0xa]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x1a
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov ax, word ptr [edi+0xc]
        _emit 0x66
        _emit 0xa3
        _emit 0x1c
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [edi+0xe]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x1e
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [edi+0x10]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x20
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov bp, word ptr [ebx+8]
        mov word ptr [esp+0x12], bp
        mov bp, word ptr [ebx+0xe]
        mov ax, word ptr [ebx]
        mov cx, word ptr [ebx+6]
        mov dx, word ptr [ebx+0xc]
        mov si, word ptr [ebx+2]
        mov word ptr [esp+0x14], bp
        mov bp, word ptr [ebx+4]
        mov word ptr [esp+0x18], bp
        mov bp, word ptr [ebx+0xa]
        mov word ptr [esp+0x1a], bp
        mov bp, word ptr [ebx+0x10]
        _emit 0x66
        _emit 0xa3
        _emit 0x58
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov ax, word ptr [esp+0x12]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x5a
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [esp+0x14]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x5c
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [esp+0x18]
        _emit 0x66
        _emit 0xa3
        _emit 0x62
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov ax, word ptr [esp+0x1a]
        _emit 0x66
        _emit 0x89
        _emit 0x35
        _emit 0x60
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x64
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
        _emit 0x2d
        _emit 0x6c
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0xe8
        _emit 0x06
        _emit 0xdc
        _emit 0xfe
        _emit 0xff
        _emit 0x66
        _emit 0x8b
        _emit 0x0d
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x8b
        _emit 0x15
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x8b
        _emit 0x2d
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov word ptr [esp+0x22], cx
        mov word ptr [esp+0x24], dx
        _emit 0xe8
        _emit 0xe2
        _emit 0xdd
        _emit 0xfe
        _emit 0xff
        _emit 0x66
        _emit 0xa1
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x8b
        _emit 0x0d
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x8b
        _emit 0x15
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov word ptr [esp+0x10], ax
        mov word ptr [esp+0x12], cx
        mov word ptr [esp+0x14], dx
        _emit 0xe8
        _emit 0x5a
        _emit 0xde
        _emit 0xfe
        _emit 0xff
        _emit 0x66
        _emit 0x8b
        _emit 0x15
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov esi, dword ptr [esp+0x2c]
        _emit 0x66
        _emit 0xa1
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x8b
        _emit 0x0d
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov word ptr [esp+0x1c], dx
        mov dx, word ptr [esp+0x22]
        mov word ptr [esi+2], dx
        mov dx, word ptr [esp+0x24]
        mov word ptr [esi+4], dx
        mov dx, word ptr [esp+0x10]
        mov word ptr [esi+6], dx
        mov dx, word ptr [esp+0x12]
        mov word ptr [esi+8], dx
        mov dx, word ptr [esp+0x14]
        mov word ptr [esi], bp
        mov word ptr [esi+0xa], dx
        mov word ptr [esi+0xc], ax
        mov ax, word ptr [esp+0x1c]
        mov word ptr [esi+0xe], cx
        mov word ptr [esi+0x10], ax
        _emit 0x66
        _emit 0x89
        _emit 0x2d
        _emit 0x10
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [esi+2]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x12
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [esi+4]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x14
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov ax, word ptr [esi+6]
        _emit 0x66
        _emit 0xa3
        _emit 0x16
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [esi+8]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x18
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [esi+0xa]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x1a
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov ax, word ptr [esi+0xc]
        _emit 0x66
        _emit 0xa3
        _emit 0x1c
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [esi+0xe]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x1e
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dx, word ptr [esi+0x10]
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x20
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov ax, word ptr [ebx+0x12]
        mov cx, word ptr [ebx+0x14]
        mov bx, word ptr [ebx+0x16]
        _emit 0x66
        _emit 0xa3
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x1d
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0xe8
        _emit 0x6a
        _emit 0xd2
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
        _emit 0x8b
        _emit 0x1d
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov ax, word ptr [edi+0x12]
        mov cx, word ptr [edi+0x14]
        _emit 0x66
        _emit 0x8b
        _emit 0x2d
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov di, word ptr [edi+0x16]
        sub ax, dx
        sub cx, bx
        sub di, bp
        mov word ptr [esi+0x12], ax
        mov word ptr [esi+0x16], di
        mov word ptr [esi+0x14], cx
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x18
        ret
    }
}
