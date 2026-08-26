__declspec(naked) void FUN_004ecf50()
{
    __asm {
        push esi
        mov esi, ecx
        mov eax, dword ptr [esi+0x58]
        test eax, eax
        je short done
        mov ecx, dword ptr [eax]
        push 0
        push 0
        push 0x004ecf90
        push eax
        call dword ptr [ecx+0x5c]
        mov eax, dword ptr [esi+0x58]
        push eax
        mov edx, dword ptr [eax]
        call dword ptr [edx+0x20]
        mov eax, dword ptr [esi+0x58]
        push eax
        mov ecx, dword ptr [eax]
        call dword ptr [ecx+8]
    done:
        mov dword ptr [esi+0x58], 0
        pop esi
        ret
    }
}
