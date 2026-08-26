__declspec(naked) void FUN_004ebfe0()
{
    __asm {
        push esi
        mov esi, ecx
        mov eax, dword ptr [esi+4]
        test eax, eax
        je short done
        mov ecx, dword ptr [eax]
        push eax
        call dword ptr [ecx+8]
    done:
        mov dword ptr [esi+4], 0
        pop esi
        ret
    }
}
