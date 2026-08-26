// Append a big-endian 16-bit value to the active output buffer.
__declspec(naked) void FUN_004fa5d0()
{
    __asm {
        mov edx, dword ptr [esp+8]
        push esi
        mov esi, dword ptr [esp+8]
        mov eax, dword ptr [esi+0x14]
        mov ecx, dword ptr [esi+8]
        mov byte ptr [eax+ecx], dh
        mov eax, dword ptr [esi+0x14]
        inc eax
        mov ecx, dword ptr [esi+8]
        mov dword ptr [esi+0x14], eax
        mov byte ptr [ecx+eax], dl
        inc dword ptr [esi+0x14]
        pop esi
        ret
    }
}
