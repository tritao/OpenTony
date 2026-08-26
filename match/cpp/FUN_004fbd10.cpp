// Clear parser token and histogram tables for a new stream.
__declspec(naked) void FUN_004fbd10()
{
    __asm {
        mov edx, dword ptr [esp+4]
        mov eax, 0x11e
        lea ecx, [edx+0x8c]
    clear_primary:
        mov word ptr [ecx], 0
        add ecx, 4
        dec eax
        jnz clear_primary
        lea ecx, [edx+0x980]
        mov eax, 0x1e
    clear_secondary:
        mov word ptr [ecx], 0
        add ecx, 4
        dec eax
        jnz clear_secondary
        lea ecx, [edx+0xa74]
        mov eax, 0x13
    clear_tertiary:
        mov word ptr [ecx], 0
        add ecx, 4
        dec eax
        jnz clear_tertiary
        mov word ptr [edx+0x48c], 1
        xor eax, eax
        mov dword ptr [edx+0x16a4], eax
        mov dword ptr [edx+0x16a0], eax
        mov dword ptr [edx+0x16ac], eax
        mov dword ptr [edx+0x1698], eax
        ret
    }
}
