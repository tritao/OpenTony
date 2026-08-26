// Start the configured FMV and enter the movie frame loop.
__declspec(naked) void FUN_004e70e0()
{
    __asm {
        _emit 0x8b
        _emit 0x15
        _emit 0xa8
        _emit 0xd4
        _emit 0x52
        _emit 0x00
        xor eax, eax
        _emit 0xa0
        _emit 0x36
        _emit 0x1d
        _emit 0x56
        _emit 0x00
        lea ecx, [eax+eax*8]
        lea ecx, [edx+ecx*4]
        mov edx, dword ptr [ecx+4]
        mov ecx, dword ptr [ecx]
        push edx
        push ecx
        push eax
        push 0x0054b278
        _emit 0xe8
        _emit 0x4b
        _emit 0x87
        _emit 0xfe
        _emit 0xff
        xor edx, edx
        _emit 0x8a
        _emit 0x15
        _emit 0x36
        _emit 0x1d
        _emit 0x56
        _emit 0x00
        push edx
        _emit 0xe8
        _emit 0x5d
        _emit 0xff
        _emit 0xff
        _emit 0xff
        push 0
        push 0
        push eax
        _emit 0xe8
        _emit 0x73
        _emit 0xf4
        _emit 0xff
        _emit 0xff
        add esp, 0x20
        test al, al
        je short done
        _emit 0xe8
        _emit 0xf7
        _emit 0xee
        _emit 0xff
        _emit 0xff
        test eax, eax
        jne short frame_tick
        // Stop playback and tail-transfer to the teardown helper.
        _emit 0xe9
        _emit 0x6e
        _emit 0xfc
        _emit 0xff
        _emit 0xff
    frame_tick:
        // Continue in the original game frame tick routine.
        _emit 0xe9
        _emit 0xa9
        _emit 0x0b
        _emit 0x01
        _emit 0x00
    done:
        ret
    }
}
