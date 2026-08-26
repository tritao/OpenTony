// Measure two fixed-point endpoints against a reference and update bounds.
__declspec(naked) int FUN_004f3f30()
{
    __asm {
        sub esp, 0x10
        mov eax, dword ptr [esp+0x18]
        push ebx
        add eax, 4
        push ebp
        push esi
        mov esi, dword ptr [esp+0x20]
        mov ebx, dword ptr [eax]
        push edi
        mov edx, dword ptr [esi+8]
        mov ebp, dword ptr [eax+4]
        sar edx, 0xc
        mov edi, dword ptr [eax+8]
        sub edx, edi
        mov dword ptr [esp+0x18], edi
        mov ecx, dword ptr [eax+0xc]
        mov eax, dword ptr [esi]
        mov dword ptr [esp+0x1c], ecx
        mov ecx, dword ptr [esi+4]
        sar eax, 0xc
        sar ecx, 0xc
        sub eax, ebx
        sub ecx, ebp
        _emit 0x66
        _emit 0xa3
        _emit 0x58
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x5a
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x5c
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0xe8
        _emit 0xab
        _emit 0xf3
        _emit 0xfe
        _emit 0xff
        _emit 0x8b
        _emit 0x15
        _emit 0x08
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov eax, dword ptr [esi+0xc]
        mov ecx, dword ptr [esi+0x10]
        _emit 0x8b
        _emit 0x3d
        _emit 0xc0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov dword ptr [esp+0x28], edx
        mov edx, dword ptr [esi+0x14]
        sar eax, 0xc
        sub eax, ebx
        mov ebx, dword ptr [esp+0x18]
        sar ecx, 0xc
        sar edx, 0xc
        sub ecx, ebp
        sub edx, ebx
        _emit 0x66
        _emit 0xa3
        _emit 0x58
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x0d
        _emit 0x5a
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x66
        _emit 0x89
        _emit 0x15
        _emit 0x5c
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0xe8
        _emit 0x66
        _emit 0xf3
        _emit 0xfe
        _emit 0xff
        _emit 0xa1
        _emit 0x08
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        _emit 0x8b
        _emit 0x0d
        _emit 0xc0
        _emit 0x3e
        _emit 0x6a
        _emit 0x00
        mov edx, dword ptr [esp+0x28]
        mov dword ptr [esi+0x1c], eax
        mov eax, edi
        mov dword ptr [esi+0x28], edi
        sar eax, 2
        cmp edi, ecx
        mov dword ptr [esi+0x18], edx
        mov dword ptr [esi+0x20], 0
        mov dword ptr [esi+0x24], 0x1000
        mov dword ptr [esi+0x2c], ecx
        pop edi
        pop esi
        pop ebp
        pop ebx
        jl short use_ecx
        sar ecx, 2
        mov eax, ecx
    use_ecx:
        lea ecx, [eax-0x1000]
        test ecx, ecx
        jl short finish
        mov eax, 0xfff
    finish:
        add esp, 0x10
        ret
    }
}
