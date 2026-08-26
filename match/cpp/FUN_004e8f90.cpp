__declspec(naked) int FUN_004e8f90()
{
    __asm {
        push edi
        mov edi, dword ptr [esp+0x10]
        mov eax, edi
        and eax, 0x8000007f
        jns short mask_done
        dec eax
        or eax, 0xffffff80
        inc eax
    mask_done:
        je short valid
        xor eax, eax
        pop edi
        ret
    valid:
        _emit 0x8b
        _emit 0x0d
        _emit 0x04
        _emit 0xb3
        _emit 0x54
        _emit 0x00
        push esi
        mov esi, dword ptr [esp+0x10]
        push 0
        push esi
        push ecx
        _emit 0xe8
        _emit 0x33
        _emit 0xea
        _emit 0xff
        _emit 0xff
        add esp, 0x0c
        cmp eax, esi
        pop esi
        je short id_ok
        xor eax, eax
        pop edi
        ret
    id_ok:
        mov edx, dword ptr [esp+8]
        _emit 0xa1
        _emit 0x04
        _emit 0xb3
        _emit 0x54
        _emit 0x00
        push edi
        push edx
        push eax
        _emit 0xe8
        _emit 0xf6
        _emit 0xea
        _emit 0xff
        _emit 0xff
        add esp, 0x0c
        test eax, eax
        jne short success
        pop edi
        ret
    success:
        _emit 0xc6
        _emit 0x05
        _emit 0xbc
        _emit 0x74
        _emit 0x6a
        _emit 0x00
        _emit 0x00
        _emit 0xc7
        _emit 0x05
        _emit 0xc0
        _emit 0x74
        _emit 0x6a
        _emit 0x00
        _emit 0x05
        _emit 0x00
        _emit 0x00
        _emit 0x00
        mov eax, 1
        pop edi
        ret
    }
}
