// Create and configure the DirectInput mouse device.
__declspec(naked) unsigned char FUN_004ec790()
{
    __asm {
        mov eax, dword ptr [esp+4]
        push esi
        mov esi, ecx
        push edi
        push 0
        mov dword ptr [esi+0x24], eax
        mov eax, dword ptr [eax+4]
        lea edi, [esi+0x28]
        mov ecx, dword ptr [eax]
        push edi
        push 0x0051a5e8
        push eax
        call dword ptr [ecx+0xc]
        test eax, eax
        jnl short mouse_interface
        push 0x262
    mouse_error_common:
        mov ecx, dword ptr [esi+0x24]
        push eax
        push 0x0054c8a8
        _emit 0xe8
        _emit 0xfa
        _emit 0xf9
        _emit 0xff
        _emit 0xff
    mouse_failure_return:
        pop edi
        _emit 0x32
        _emit 0xc0
        pop esi
        ret 4
    mouse_interface:
        mov edx, dword ptr [esi+0x24]
        push 0x00519aa8
        mov dword ptr [edx+0xc], esi
        mov eax, dword ptr [edi]
        push eax
        mov ecx, dword ptr [eax]
        call dword ptr [ecx+0x2c]
        test eax, eax
        jnl short mouse_acquire
        push 0x26c
        jmp short mouse_error_common
    mouse_acquire:
        mov eax, dword ptr [esi+0x24]
        mov edi, dword ptr [edi]
        push 5
        mov ecx, dword ptr [eax+0x14]
        mov edx, dword ptr [edi]
        push ecx
        push edi
        call dword ptr [edx+0x34]
        test eax, eax
        jnl short mouse_ready
        push 0x275
        jmp short mouse_error_common
    mouse_ready:
        mov ecx, esi
        _emit 0xe8
        _emit 0x52
        _emit 0x00
        _emit 0x00
        _emit 0x00
        test al, al
        je short mouse_failure_return
        mov ecx, esi
        _emit 0xe8
        _emit 0xa7
        _emit 0x00
        _emit 0x00
        _emit 0x00
        test al, al
        pop edi
        pop esi
        setnz al
        ret 4
    }
}
