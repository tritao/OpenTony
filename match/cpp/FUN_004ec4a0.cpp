__declspec(naked) void FUN_004ec4a0()
{
    __asm {
        mov eax, ecx
        xor ecx, ecx
        mov dword ptr [eax], 0x00519a1c
        mov byte ptr [eax+0x10c], cl
        mov dword ptr [eax+0x108], ecx
        ret
    }
}
