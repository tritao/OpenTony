// Seek within a buffered or XA-backed media descriptor.
__declspec(naked) int FUN_004e79f0()
{
    __asm {
        push esi
        mov esi, dword ptr [esp+8]
        cmp esi, -1
        je invalid
        mov al, byte ptr [esi+0x6a6d10]
        test al, al
        je invalid
        cmp al, 2
        mov eax, dword ptr [esp+0x10]
        jne short buffered
        test eax, eax
        jne short xa_cur
        mov ecx, dword ptr [esi*4+0x006a6d1c]
        push eax
        mov eax, dword ptr [esp+0x10]
        push eax
        push ecx
        jmp short xa_seek
    xa_cur:
        cmp eax, 1
        jne short xa_end
        mov edx, dword ptr [esp+0xc]
        push eax
        mov eax, dword ptr [esi*4+0x006a6d1c]
        push edx
        push eax
        jmp short xa_seek
    xa_end:
        cmp eax, 2
        jne short xa_done
        mov ecx, dword ptr [esp+0xc]
        mov edx, dword ptr [esi*4+0x006a6d1c]
        push eax
        push ecx
        push edx
    xa_seek:
        _emit 0xe8
        _emit 0x4a
        _emit 0xa8
        _emit 0x01
        _emit 0x00
        add esp, 0xc
    xa_done:
        mov eax, dword ptr [esi*4+0x006a6d1c]
        push eax
        _emit 0xe8
        _emit 0xb7
        _emit 0xa6
        _emit 0x01
        _emit 0x00
        add esp, 4
        pop esi
        ret
    buffered:
        test eax, eax
        jne short buffered_cur
        mov ecx, dword ptr [esp+0xc]
        mov dword ptr [esi*4+0x006a716c], ecx
        mov eax, ecx
        pop esi
        ret
    buffered_cur:
        cmp eax, 1
        jne short buffered_end
        mov edx, dword ptr [esp+0xc]
        mov eax, dword ptr [esi*4+0x006a716c]
        add eax, edx
        mov dword ptr [esi*4+0x006a716c], eax
        pop esi
        ret
    buffered_end:
        cmp eax, 2
        jne short buffered_done
        mov eax, dword ptr [esi*4+0x006a6d44]
        mov ecx, dword ptr [esp+0xc]
        sub eax, ecx
        mov dword ptr [esi*4+0x006a716c], eax
    buffered_done:
        mov eax, dword ptr [esi*4+0x006a716c]
        pop esi
        ret
    invalid:
        push 0x0054b364
        _emit 0xe8
        _emit 0x8c
        _emit 0x7d
        _emit 0xfe
        _emit 0xff
        add esp, 4
        or eax, -1
        pop esi
        ret
    }
}
