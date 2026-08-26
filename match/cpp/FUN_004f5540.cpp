// Transform a nine-word box through the shared orientation routine.
__declspec(naked) void FUN_004f5540()
{
    __asm {
        sub esp, 0x18
        push ebx
        push ebp
        push esi
        mov esi, dword ptr [esp+0x2c]
        mov eax, dword ptr [esp+0x28]
        push edi
        mov bp, word ptr [esi+0xc]
        mov cx, word ptr [esi]
        mov word ptr [esp+0x1c], bp
        mov bp, word ptr [esi+2]
        mov word ptr [esp+0x10], bp
        mov bp, word ptr [esi+8]
        mov word ptr [esp+0x12], bp
        mov bp, word ptr [esi+0xe]
        mov di, word ptr [eax+0x2a]
        mov bx, word ptr [eax+0x2c]
        mov dx, word ptr [esi+6]
        mov ax, word ptr [eax+0x28]
        mov word ptr [esp+0x14], bp
        mov bp, word ptr [esi+4]
        mov word ptr [esp+0x20], bp
        mov bp, word ptr [esi+0xa]
        mov word ptr [esp+0x22], bp
        mov bp, word ptr [esi+0x10]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [esp+0x1c]
        _emit 0x66
        _emit 0xa3
        _emit 0x34
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0xe8
        _emit 0xdf
        _emit 0xcd
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
        mov dx, word ptr [esp+0x10]
        mov word ptr [esp+0x1a], ax
        mov ax, word ptr [esp+0x12]
        mov word ptr [esp+0x1c], cx
        mov cx, word ptr [esp+0x14]
        _emit 0x66
        _emit 0x89
        _emit 0x3d
        _emit 0x34
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0xa3
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0xe8
        _emit 0x8d
        _emit 0xcd
        _emit 0xfe
        _emit 0xff
        _emit 0x66
        _emit 0x8b
        _emit 0x15
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0xa1
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov cx, word ptr [esp+0x20]
        _emit 0x66
        _emit 0x8b
        _emit 0x3d
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov word ptr [esp+0x12], dx
        mov dx, word ptr [esp+0x22]
        mov word ptr [esp+0x14], ax
        _emit 0x66
        _emit 0x89
        _emit 0x1d
        _emit 0x34
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x2d
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0xe8
        _emit 0x44
        _emit 0xcd
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
        mov bx, word ptr [esp+0x18]
        mov word ptr [esi+2], di
        mov di, word ptr [esp+0x12]
        mov word ptr [esi], bx
        mov bx, word ptr [esp+0x1a]
        mov word ptr [esi+8], di
        mov di, word ptr [esp+0x14]
        mov word ptr [esi+6], bx
        mov bx, word ptr [esp+0x1c]
        mov word ptr [esi+0xe], di
        mov word ptr [esi+0xc], bx
        mov word ptr [esi+4], ax
        mov word ptr [esi+0xa], cx
        mov word ptr [esi+0x10], dx
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x18
        ret
    }
}
