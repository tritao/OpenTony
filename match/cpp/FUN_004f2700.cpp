// Return the CPUID feature bit used by the sound/render path.
__declspec(naked) unsigned int FUN_004f2700()
{
    __asm {
        push ebp
        mov ebp, esp
        push ecx
        push ebx
        mov eax, 1
        cpuid
        mov dword ptr [ebp-4], edx
        mov eax, dword ptr [ebp-4]
        pop ebx
        shr eax, 0x17
        and al, 1
        mov esp, ebp
        pop ebp
        ret
    }
}
