// Pack fixed-point vector samples into the engine's 16-bit stream format.
__declspec(naked) void FUN_004ef1a0()
{
    __asm {
        push ebp
        mov ebp, esp
        push ecx
        mov eax, dword ptr [ebp+8]
        xor ecx, ecx
        push edi
        mov dword ptr [ebp+8], eax
        mov cx, word ptr [eax+0x4a]
        mov dword ptr [ebp-4], ecx
        mov edi, dword ptr [ebp+0xc]
        mov ecx, dword ptr [ebp-4]
        push ebp
        mov ebp, dword ptr [ebp+8]
        _emit 0x0f
        _emit 0x6f
        _emit 0x6d
        _emit 0x00
        _emit 0x0f
        _emit 0xeb
        _emit 0xf6
        _emit 0x0f
        _emit 0x7f
        _emit 0xec
        _emit 0x0f
        _emit 0xeb
        _emit 0xf6
        _emit 0x0f
        _emit 0x71
        _emit 0xd4
        _emit 0x0a
        _emit 0x0f
        _emit 0xeb
        _emit 0xf6
    pack_loop:
        _emit 0x0f
        _emit 0x7f
        _emit 0xe2
        _emit 0x0f
        _emit 0x73
        _emit 0xd4
        _emit 0x0a
        _emit 0x0f
        _emit 0xeb
        _emit 0xd4
        _emit 0x0f
        _emit 0x73
        _emit 0xd4
        _emit 0x0b
        _emit 0x0f
        _emit 0xfd
        _emit 0x6d
        _emit 0x08
        _emit 0x0f
        _emit 0xeb
        _emit 0xd4
        _emit 0x0f
        _emit 0x7f
        _emit 0xec
        add edi, 2
        _emit 0x0f
        _emit 0x7e
        _emit 0xd0
        _emit 0x0f
        _emit 0x71
        _emit 0xd4
        _emit 0x0a
        mov word ptr [edi-2], ax
        dec ecx
        jnz pack_loop
        nop
        pop ebp
        pop edi
        mov esp, ebp
        pop ebp
        ret
    }
}
