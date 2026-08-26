// Close Bink/PCM movie resources and clear the movie playback state.
__declspec(naked) void FUN_004e6da0()
{
    __asm {
        _emit 0xa1
        _emit 0xe4
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        test eax, eax
        je short no_bink
        push eax
        // BinkClose import thunk.
        _emit 0xff
        _emit 0x15
        _emit 0x14
        _emit 0x83
        _emit 0x51
        _emit 0x00
        _emit 0xc7
        _emit 0x05
        _emit 0xe4
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0x00
    no_bink:
        _emit 0xa0
        _emit 0xe1
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        push esi
        test al, al
        je short clear_movie_flag
        _emit 0xa1
        _emit 0xa0
        _emit 0xa3
        _emit 0x9d
        _emit 0x02
        test eax, eax
        je short pcm_release
        _emit 0xa1
        _emit 0xa0
        _emit 0x6b
        _emit 0x6a
        _emit 0x00
        push eax
        _emit 0xe8
        _emit 0x1b
        _emit 0x95
        _emit 0x01
        _emit 0x00
        add esp, 4
        jmp short clear_buffers
    pcm_release:
        _emit 0xa1
        _emit 0xe8
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        test eax, eax
        je short clear_buffers
        mov ecx, dword ptr [eax]
        push eax
        call dword ptr [ecx+8]
        mov esi, eax
        test esi, esi
        je short clear_pcm
        push 0x436
        push 0x0054b198
        push esi
        _emit 0xe8
        _emit 0x5e
        _emit 0x8d
        _emit 0xfe
        _emit 0xff
        push 1
        _emit 0xe8
        _emit 0x77
        _emit 0xa1
        _emit 0xfe
        _emit 0xff
        add esp, 0x10
        push esi
        _emit 0xe8
        _emit 0x27
        _emit 0xa6
        _emit 0x01
        _emit 0x00
    clear_pcm:
        _emit 0xc7
        _emit 0x05
        _emit 0xe8
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0x00
    clear_buffers:
        push edi
        mov ecx, 0xb
        xor eax, eax
        mov edi, 0x006a6b98
        rep stosd
        mov ecx, 0xb
        mov edi, 0x006a69f8
        rep stosd
        pop edi
    clear_movie_flag:
        _emit 0xc6
        _emit 0x05
        _emit 0xfd
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        _emit 0x00
        pop esi
        ret
    }
}
