// Builds the XA track label and starts playback for one track.
__declspec(naked) void FUN_004e7310(int track, int sector)
{
    __asm {
        sub esp, 0x14
        // Keep VC6's original four-byte LEA encoding (rather than 8d 04 24).
        _emit 0x8d
        _emit 0x44
        _emit 0x24
        _emit 0x00
        push ebx
        mov ebx, dword ptr [esp+0x1c]
        push esi
        push edi
        mov edi, dword ptr [esp+0x28]
        push 0x0054b230
        lea esi, [edi+ebx*8]
        push esi
        push 0x0054b2fc
        push 0x0054b2f0
        push eax
        // _snprintf(local, 20, "%s%02i%s", ...)
        _emit 0xe8
        _emit 0x66
        _emit 0x8f
        _emit 0x01
        _emit 0x00
        lea ecx, [esp+0x20]
        lea edx, [esi-0x10]
        push ecx
        push edx
        push esi
        push edi
        push ebx
        push 0x0054b2cc
        // Play XA: logger/formatter at 0x004cf850.
        _emit 0xe8
        _emit 0xff
        _emit 0x84
        _emit 0xfe
        _emit 0xff
        lea eax, [esp+0x38]
        push eax
        // Release the temporary track string at 0x004e7260.
        _emit 0xe8
        _emit 0x05
        _emit 0xff
        _emit 0xff
        _emit 0xff
        add esp, 0x30
        pop edi
        pop esi
        pop ebx
        add esp, 0x14
        ret
    }
}
