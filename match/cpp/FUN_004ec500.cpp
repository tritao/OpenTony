// Create and configure the DirectInput keyboard device.
__declspec(naked) unsigned char FUN_004ec500()
{
    __asm {
        mov eax, dword ptr [esp+4]
        push esi
        mov esi, ecx
        push edi
        push 0
        mov dword ptr [esi+0x104], eax
        mov eax, dword ptr [eax+4]
        lea edi, [esi+0x108]
        mov ecx, dword ptr [eax]
        push edi
        push 0x0051a5d8
        push eax
        call dword ptr [ecx+0xc]
        test eax, eax
        jnl short keyboard_interface
        push 0x1a6
        jmp short keyboard_error
    keyboard_interface:
        mov edx, dword ptr [esi+0x104]
        push 0x00519a90
        mov dword ptr [edx+8], esi
        mov eax, dword ptr [edi]
        push eax
        mov ecx, dword ptr [eax]
        call dword ptr [ecx+0x2c]
        test eax, eax
        jnl short keyboard_acquire
        push 0x1b0
        jmp short keyboard_error
    keyboard_acquire:
        mov eax, dword ptr [esi+0x104]
        mov edi, dword ptr [edi]
        push 5
        mov ecx, dword ptr [eax+0x14]
        mov edx, dword ptr [edi]
        push ecx
        push edi
        call dword ptr [edx+0x34]
        test eax, eax
        jnl short keyboard_ok
        push 0x1bd
    keyboard_error:
        mov ecx, dword ptr [esi+0x104]
        push eax
        push 0x0054c850
        _emit 0xe8
        _emit 0x41
        _emit 0xfc
        _emit 0xff
        _emit 0xff
        pop edi
        _emit 0x32
        _emit 0xc0
        pop esi
        ret 4
    keyboard_ok:
        pop edi
        mov al, 1
        pop esi
        ret 4
    }
}
