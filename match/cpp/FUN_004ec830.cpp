__declspec(naked) void FUN_004ec830()
{
    __asm {
        push esi
        mov esi, ecx
        mov eax, dword ptr [esi+0x28]
        test eax, eax
        je short done
        mov ecx, dword ptr [eax]
        push eax
        call dword ptr [ecx+0x20]
        mov eax, dword ptr [esi+0x28]
        push eax
        mov edx, dword ptr [eax]
        call dword ptr [edx+8]
    done:
        mov dword ptr [esi+0x28], 0
        pop esi
        ret
    }
}
