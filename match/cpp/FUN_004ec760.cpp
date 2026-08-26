__declspec(naked) void FUN_004ec760()
{
    __asm {
        xor eax, eax
        mov dword ptr [ecx+0x30], eax
        mov dword ptr [ecx+0x34], eax
        mov dword ptr [ecx+4], eax
        mov dword ptr [ecx+8], eax
        mov dword ptr [ecx+0x10], eax
        mov dword ptr [ecx+0x38], eax
        mov dword ptr [ecx+0x3c], eax
        mov dword ptr [ecx+0x40], eax
        lea eax, [ecx+0x44]
        xor ecx, ecx
        mov dword ptr [eax], ecx
        mov dword ptr [eax+4], ecx
        mov dword ptr [eax+8], ecx
        ret
    }
}
