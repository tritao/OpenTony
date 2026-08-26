// Set the joystick auto-center property through its device vtable.
__declspec(naked) unsigned char FUN_004ed250()
{
    __asm {
        sub esp, 0x14
        push esi
        mov esi, ecx
        mov cl, byte ptr [esp+0x1c]
        xor eax, eax
        test cl, cl
        setnz al
        mov dword ptr [esp+0x14], eax
        mov eax, dword ptr [esi+0x58]
        lea edx, [esp+4]
        mov dword ptr [esp+4], 0x14
        mov dword ptr [esp+8], 0x10
        mov dword ptr [esp+0xc], 0
        mov dword ptr [esp+0x10], 0
        mov ecx, dword ptr [eax]
        push edx
        push 9
        push eax
        call dword ptr [ecx+0x18]
        test eax, eax
        jnl short auto_center_ok
        mov ecx, dword ptr [esi+0x54]
        push 0x4f4
        push eax
        push 0x0054c984
        _emit 0xe8
        _emit 0x14
        _emit 0xef
        _emit 0xff
        _emit 0xff
        _emit 0x32
        _emit 0xc0
        pop esi
        add esp, 0x14
        ret 4
    auto_center_ok:
        mov al, 1
        pop esi
        add esp, 0x14
        ret 4
    }
}
