__declspec(naked) void FUN_004ec590()
{
    __asm {
        push esi
        mov esi, ecx
        mov eax, dword ptr [esi+0x108]
        test eax, eax
        je short done
        mov ecx, dword ptr [eax]
        push eax
        call dword ptr [ecx+0x20]
        mov eax, dword ptr [esi+0x108]
        push eax
        mov edx, dword ptr [eax]
        call dword ptr [edx+8]
        mov dword ptr [esi+0x108], 0
    done:
        pop esi
        ret
    }
}
