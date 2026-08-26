// Read from a media descriptor, dispatching XA handles and advancing buffers.
__declspec(naked) unsigned int FUN_004e7ad0(int descriptor, void *buffer, unsigned int size)
{
    __asm {
        mov edx, dword ptr [esp+4]
        cmp edx, -1
        je short invalid
        _emit 0x8a
        _emit 0x82
        _emit 0x10
        _emit 0x6d
        _emit 0x6a
        _emit 0x00
        test al, al
        je short invalid
        cmp al, 2
        jne short buffered
        _emit 0x8b
        _emit 0x04
        _emit 0x95
        _emit 0x1c
        _emit 0x6d
        _emit 0x6a
        _emit 0x00
        mov ecx, dword ptr [esp+0xc]
        mov edx, dword ptr [esp+8]
        push eax
        push ecx
        push 1
        push edx
        // Read XA data through the original media callback.
        _emit 0xe8
        _emit 0x58
        _emit 0xa8
        _emit 0x01
        _emit 0x00
        add esp, 0x10
        ret
    buffered:
        mov eax, dword ptr [esp+0xc]
        push ebx
        push esi
        _emit 0x8b
        _emit 0x34
        _emit 0x95
        _emit 0x94
        _emit 0x71
        _emit 0x6a
        _emit 0x00
        push edi
        _emit 0x8b
        _emit 0x3c
        _emit 0x95
        _emit 0x6c
        _emit 0x71
        _emit 0x6a
        _emit 0x00
        mov ecx, eax
        add esi, edi
        mov edi, dword ptr [esp+0x14]
        mov ebx, ecx
        shr ecx, 2
        rep movsd
        mov ecx, ebx
        and ecx, 3
        rep movsb
        _emit 0x8b
        _emit 0x0c
        _emit 0x95
        _emit 0x6c
        _emit 0x71
        _emit 0x6a
        _emit 0x00
        pop edi
        add ecx, eax
        pop esi
        _emit 0x89
        _emit 0x0c
        _emit 0x95
        _emit 0x6c
        _emit 0x71
        _emit 0x6a
        _emit 0x00
        pop ebx
        ret
    invalid:
        push 0x0054b380
        _emit 0xe8
        _emit 0x03
        _emit 0x7d
        _emit 0xfe
        _emit 0xff
        add esp, 4
        or eax, -1
        ret
    }
}
