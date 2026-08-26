__declspec(naked) void FUN_004ed4c0()
{
    __asm {
        mov eax, dword ptr [ecx+8]
        test eax, eax
        je short done
        mov dl, byte ptr [ecx+0x1b0]
        test dl, dl
        je short done
        mov ecx, dword ptr [eax]
        push eax
        call dword ptr [ecx+0x20]
    done:
        ret
    }
}
