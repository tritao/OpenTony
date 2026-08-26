// Test an integer box against the shared collision plane/volume query.
__declspec(naked) int FUN_004f4240()
{
    __asm {
        _emit 0x66
        _emit 0x8b
        _emit 0x44
        _emit 0x24
        _emit 0x34
        _emit 0x66
        _emit 0x8b
        _emit 0x4c
        _emit 0x24
        _emit 0x38
        _emit 0x66
        _emit 0x8b
        _emit 0x54
        _emit 0x24
        _emit 0x3c
        _emit 0x66
        _emit 0xa3
        _emit 0x10
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov eax, dword ptr [esp+0x1c]
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x18
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov ecx, dword ptr [esp+0x10]
        push ebx
        push ebp
        push esi
        cmp ecx, eax
        push edi
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x20
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        jng reject
        mov ecx, dword ptr [esp+0x30]
        mov eax, dword ptr [esp+0x24]
        cmp eax, ecx
        jng reject
        mov ebp, dword ptr [esp+0x34]
        mov eax, dword ptr [esp+0x28]
        cmp eax, ebp
        jng reject
        mov esi, dword ptr [esp+0x38]
        mov edi, dword ptr [esp+0x14]
        mov edx, dword ptr [esp+0x18]
        mov ebx, dword ptr [esp+0x40]
        mov ecx, dword ptr [esp+0x1c]
        sub esi, edi
        mov edi, dword ptr [esp+0x3c]
        sub ebx, ecx
        sub edi, edx
        test esi, esi
        jl reject
        test edi, edi
        jl reject
        test ebx, ebx
        jl reject
        _emit 0x66
        _emit 0x89
        _emit 0x35
        _emit 0xb0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x3d
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
        _emit 0x99
        _emit 0xec
        _emit 0xfe
        _emit 0xff
        mov edx, dword ptr [esp+0x14]
        mov ecx, dword ptr [esp+0x2c]
        _emit 0xa1
        _emit 0xb8
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        sub ecx, edx
        mov edx, dword ptr [esp+0x18]
        mov dword ptr [esp+0x2c], ecx
        mov ecx, dword ptr [esp+0x30]
        sub ecx, edx
        mov edx, dword ptr [esp+0x1c]
        sub ebp, edx
        test eax, eax
        jge short third_axis
        _emit 0xa1
        _emit 0xbc
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        test eax, eax
        jge short second_axis

    first_axis:
        _emit 0x66
        _emit 0x89
        _emit 0x35
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
        _emit 0x2d
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0xe8
        _emit 0x4f
        _emit 0xec
        _emit 0xfe
        _emit 0xff
        _emit 0xa1
        _emit 0xbc
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x8b
        _emit 0x0d
        _emit 0xc0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        test eax, eax
        jl reject
        xor eax, eax
        pop edi
        test ecx, ecx
        pop esi
        pop ebp
        setng al
        pop ebx
        ret

    second_axis:
        _emit 0x66
        _emit 0x8b
        _emit 0x44
        _emit 0x24
        _emit 0x2c
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0xb2
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0xa3
        _emit 0xb0
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
        _emit 0x12
        _emit 0xec
        _emit 0xfe
        _emit 0xff
        _emit 0xa1
        _emit 0xb8
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x8b
        _emit 0x0d
        _emit 0xbc
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        test eax, eax
        jl reject
        xor eax, eax
        pop edi
        test ecx, ecx
        pop esi
        pop ebp
        setng al
        pop ebx
        ret

    third_axis:
        _emit 0xa1
        _emit 0xc0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        test eax, eax
        jge short first_axis
        _emit 0x66
        _emit 0x8b
        _emit 0x4c
        _emit 0x24
        _emit 0x2c
        _emit 0x66
        _emit 0x89
        _emit 0x3d
        _emit 0xb2
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
        _emit 0x2d
        _emit 0xb4
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0xe8
        _emit 0xcf
        _emit 0xeb
        _emit 0xfe
        _emit 0xff
        _emit 0xa1
        _emit 0xb8
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x8b
        _emit 0x0d
        _emit 0xc0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        test eax, eax
        jg reject
        xor eax, eax
        pop edi
        test ecx, ecx
        pop esi
        pop ebp
        setnl al
        pop ebx
        ret

    reject:
        pop edi
        pop esi
        pop ebp
        xor eax, eax
        pop ebx
        ret
    }
}
