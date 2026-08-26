// Compress a buffer with the engine's zlib wrapper, returning its allocation.
__declspec(naked) void *FUN_004e92c0(const void *input, unsigned int input_size,
                                     unsigned int *compressed_size)
{
    __asm {
        push ebx
        push ebp
        push esi
        mov esi, dword ptr [esp+0x14]
        xor ebp, ebp
        lea eax, [esi+esi*2]
        cdq
        sub eax, edx
        sar eax, 1
        push eax
        mov dword ptr [esp+0x18], eax
        // Allocate the initial 1.5x destination buffer.
        _emit 0xe8
        _emit 0xd9
        _emit 0x88
        _emit 0x01
        _emit 0x00
        mov ebx, eax
        mov eax, dword ptr [esp+0x14]
        push esi
        lea ecx, [esp+0x1c]
        push eax
        push ecx
        push ebx
        // compress2(destination, &capacity, source, source_size)
        _emit 0xe8
        _emit 0x42
        _emit 0x0a
        _emit 0x01
        _emit 0x00
        add esp, 0x14
        cmp eax, -5
        je short destination_error
        cmp eax, -4
        je short allocation_error
        test eax, eax
        je short compressed
        push eax
        push 0x0054b4a4
        _emit 0xe8
        _emit 0x7e
        _emit 0x93
        _emit 0x01
        _emit 0x00
        add esp, 8
        push ebx
        _emit 0xe8
        _emit 0x59
        _emit 0x88
        _emit 0x01
        _emit 0x00
        add esp, 4
        mov eax, ebp
        pop esi
        pop ebp
        pop ebx
        ret
    compressed:
        mov edx, dword ptr [esp+0x14]
        push edx
        // Allocate the exact compressed size and copy the result.
        _emit 0xe8
        _emit 0x8e
        _emit 0x88
        _emit 0x01
        _emit 0x00
        mov ebp, eax
        add esp, 4
        test ebp, ebp
        je short copy_done
        mov ecx, dword ptr [esp+0x14]
        push edi
        mov eax, ecx
        mov esi, ebx
        mov edi, ebp
        shr ecx, 2
        rep movsd
        mov ecx, eax
        and ecx, 3
        rep movsb
        pop edi
    copy_done:
        mov eax, dword ptr [esp+0x18]
        test eax, eax
        je short release_initial
        mov ecx, dword ptr [esp+0x14]
        push ebx
        mov dword ptr [eax], ecx
        _emit 0xe8
        _emit 0x11
        _emit 0x88
        _emit 0x01
        _emit 0x00
        add esp, 4
        mov eax, ebp
        pop esi
        pop ebp
        pop ebx
        ret
    allocation_error:
        push 0x0054b468
        jmp short report_error
    destination_error:
        push 0x0054b420
    report_error:
        _emit 0xe8
        _emit 0x13
        _emit 0x93
        _emit 0x01
        _emit 0x00
        add esp, 4
    release_initial:
        push ebx
        _emit 0xe8
        _emit 0xee
        _emit 0x87
        _emit 0x01
        _emit 0x00
        add esp, 4
        mov eax, ebp
        pop esi
        pop ebp
        pop ebx
        ret
    }
}
