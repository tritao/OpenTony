// Advance and flush parser input according to the requested end mode.
__declspec(naked) unsigned char FUN_004fa990()
{
    __asm {
        push ebx
        push esi
        push edi
        mov ebx, 0xffff
        mov esi, dword ptr [esp+0x10]
        push ebp
        mov eax, dword ptr [esi+0xc]
        sub eax, 5
        cmp eax, ebx
        jnc have_limit
        mov ebx, eax
    have_limit:
        mov edi, dword ptr [esp+0x18]
        xor ebp, ebp
    pump:
        cmp dword ptr [esi+0x6c], 1
        ja pump_ready
        push esi
        _emit 0xe8
        _emit 0x45
        _emit 0x01
        _emit 0x00
        _emit 0x00
        add esp, 4
        mov eax, dword ptr [esi+0x6c]
        test eax, eax
        jnz pump_ready
        test edi, edi
        jz done
        test eax, eax
        jz no_input
    pump_ready:
        mov ecx, dword ptr [esi+0x6c]
        mov eax, dword ptr [esi+0x54]
        add ecx, dword ptr [esi+0x64]
        mov dword ptr [esi+0x6c], ebp
        lea edx, [eax+ebx]
        mov dword ptr [esi+0x64], ecx
        jz flush_pending
        cmp edx, ecx
        ja delayed_flush
    flush_pending:
        sub ecx, edx
        mov dword ptr [esi+0x64], edx
        mov dword ptr [esi+0x6c], ecx
        test eax, eax
        mov ecx, 0
        jl have_source
        mov ecx, dword ptr [esi+0x30]
        add ecx, eax
    have_source:
        push ebp
        sub edx, eax
        push edx
        push ecx
        push esi
        _emit 0xe8
        _emit 0x42
        _emit 0x16
        _emit 0x00
        _emit 0x00
        add esp, 0x10
        mov ecx, dword ptr [esi+0x64]
        mov edx, dword ptr [esi]
        mov dword ptr [esi+0x54], ecx
        push edx
        _emit 0xe8
        _emit 0xe1
        _emit 0xfb
        _emit 0xff
        _emit 0xff
        add esp, 4
        mov ecx, dword ptr [esi]
        cmp dword ptr [ecx+0x10], ebp
        jz final_return
    delayed_flush:
        mov eax, dword ptr [esi+0x54]
        mov ecx, dword ptr [esi+0x64]
        sub ecx, eax
        mov edx, dword ptr [esi+0x24]
        sub edx, 0x106
        cmp edx, ecx
        ja pump
        test eax, eax
        jl zero_source
        add eax, dword ptr [esi+0x30]
        jmp have_flush_source
    zero_source:
        xor eax, eax
    have_flush_source:
        push ebp
        push ecx
        push eax
        push esi
        _emit 0xe8
        _emit 0xf6
        _emit 0x15
        _emit 0x00
        _emit 0x00
        add esp, 0x10
        mov eax, dword ptr [esi+0x64]
        mov ecx, dword ptr [esi]
        mov dword ptr [esi+0x54], eax
        push ecx
        _emit 0xe8
        _emit 0x95
        _emit 0xfb
        _emit 0xff
        _emit 0xff
        add esp, 4
        mov ecx, dword ptr [esi]
        cmp dword ptr [ecx+0x10], ebp
        jnz pump
    finished:
        xor eax, eax
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    done:
        xor eax, eax
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    no_input:
        mov ecx, dword ptr [esi+0x54]
        mov edx, 0
        test ecx, ecx
        jl no_input_source
        mov edx, dword ptr [esi+0x30]
        add edx, ecx
    no_input_source:
        lea eax, [edi-4]
        cmp eax, 1
        sbb eax, eax
        neg eax
        push eax
        mov eax, dword ptr [esi+0x64]
        sub eax, ecx
        push eax
        push edx
        push esi
        _emit 0xe8
        _emit 0xa0
        _emit 0x15
        _emit 0x00
        _emit 0x00
        add esp, 0x10
        mov ecx, dword ptr [esi+0x64]
        mov edx, dword ptr [esi]
        mov dword ptr [esi+0x54], ecx
        push edx
        _emit 0xe8
        _emit 0x3f
        _emit 0xfb
        _emit 0xff
        _emit 0xff
        add esp, 4
        mov ecx, dword ptr [esi]
        cmp dword ptr [ecx+0x10], 0
        jnz return_mode
        sub edi, 4
        pop ebp
        cmp edi, 1
        pop edi
        sbb eax, eax
        pop esi
        and eax, 2
        pop ebx
        ret
    return_mode:
        sub edi, 4
        pop ebp
        cmp edi, 1
        pop edi
        sbb eax, eax
        pop esi
        and eax, 2
        pop ebx
        inc eax
        ret
    final_return:
        xor eax, eax
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    }
}
