// Start FMV playback and initialize the movie frame callback.
__declspec(naked) unsigned char FUN_004e7260()
{
    __asm {
        _emit 0xe8
        _emit 0x0b
        _emit 0x01
        _emit 0x00
        _emit 0x00
        _emit 0xe8
        _emit 0x76
        _emit 0x0a
        _emit 0x01
        _emit 0x00
        mov eax, dword ptr [esp+4]
        push 0
        push 1
        push eax
        _emit 0xc6
        _emit 0x05
        _emit 0xe0
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        _emit 0x01
        _emit 0xe8
        _emit 0x11
        _emit 0xf3
        _emit 0xff
        _emit 0xff
        add esp, 0xc
        test al, al
        jne short movie_ready
        _emit 0xa2
        _emit 0xe0
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        ret
    movie_ready:
        _emit 0xe8
        _emit 0x8f
        _emit 0xed
        _emit 0xff
        _emit 0xff
        test eax, eax
        jne short callback_ready
        _emit 0xe8
        _emit 0x06
        _emit 0xfb
        _emit 0xff
        _emit 0xff
        _emit 0xc6
        _emit 0x05
        _emit 0xe0
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        _emit 0x00
        _emit 0x32
        _emit 0xc0
        ret
    callback_ready:
        push 0
        push 0
        push 0x004e71d0
        _emit 0xc6
        _emit 0x05
        _emit 0xa8
        _emit 0xb0
        _emit 0x54
        _emit 0x00
        _emit 0x00
        _emit 0xe8
        _emit 0x9e
        _emit 0xab
        _emit 0x01
        _emit 0x00
        add esp, 0xc
        _emit 0xe8
        _emit 0x1f
        _emit 0x0a
        _emit 0x01
        _emit 0x00
        _emit 0xa0
        _emit 0xe0
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        ret
    }
}
