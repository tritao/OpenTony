__declspec(naked) void FUN_004ef380()
{
    __asm {
        mov eax, dword ptr [esp+8]
        mov ecx, dword ptr [esp+4]
        mov dword ptr [eax+4], 0
        mov edx, dword ptr [ecx+4]
        mov dword ptr [eax], edx
        mov dword ptr [ecx+4], eax
        mov ecx, dword ptr [eax]
        test ecx, ecx
        je short done
        mov dword ptr [ecx+4], eax
    done:
        ret
    }
}
