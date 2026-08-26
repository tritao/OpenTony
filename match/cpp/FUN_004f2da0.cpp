// Release all eight channels belonging to one sound-bank slot.
__declspec(naked) void FUN_004f2da0()
{
    __asm {
        _emit 0xa1
        _emit 0x60
        _emit 0x83
        _emit 0x9d
        _emit 0x02
        push ebx
        push ebp
        push esi
        test eax, eax
        push edi
        je short done
        mov esi, dword ptr [esp+0x14]
        push esi
        _emit 0xe8
        _emit 0x09
        _emit 0x0a
        _emit 0x00
        _emit 0x00
        lea ebp, [esi+esi*4]
        add esp, 4
        xor ebx, ebx
        shl ebp, 3
        lea esi, [ebp+0x029d6920]
    channel_loop:
        mov eax, dword ptr [esi]
        test eax, eax
        je short channel_done
        mov ecx, dword ptr [eax]
        push eax
        call dword ptr [ecx+8]
        mov edi, eax
        test edi, edi
        jnz short release_error
        mov dword ptr [esi], eax
    channel_done:
        inc ebx
        add esi, 4
        cmp ebx, 8
        jl short channel_loop
        _emit 0xc7
        _emit 0x85
        _emit 0x40
        _emit 0x69
        _emit 0x9d
        _emit 0x02
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0x00
    done:
        pop edi
        pop esi
        pop ebp
        pop ebx
        ret
    release_error:
        push 0x72c
        push 0x0055bba0
        push edi
        _emit 0xe8
        _emit 0xac
        _emit 0xcb
        _emit 0xfd
        _emit 0xff
        push 1
        _emit 0xe8
        _emit 0x75
        _emit 0xe1
        _emit 0xfd
        _emit 0xff
        add esp, 0x10
        push edi
        _emit 0xe8
        _emit 0x25
        _emit 0xe6
        _emit 0x00
        _emit 0x00
    }
}
