__declspec(naked) int FUN_004ec160()
{
    __asm {
        push esi
        mov esi, dword ptr [esp+8]
        mov edx, dword ptr [esp+0xc]
        mov eax, dword ptr [esi+0x24]
        and eax, 0xf
        cmp al, 4
        jne short count
        mov byte ptr [edx+0x245d], 1
    count:
        mov eax, dword ptr [edx+0x2458]
        cmp eax, 0xf
        jge short zero
        test esi, esi
        je short zero
        inc eax
        push edi
        mov dword ptr [edx+0x2458], eax
        mov ecx, dword ptr [esi]
        lea edi, [eax+eax*8]
        shl edi, 4
        add edi, eax
        mov eax, 1
        lea edi, [edx+edi*4+0x18]
        mov edx, ecx
        shr ecx, 2
        rep movsd
        mov ecx, edx
        and ecx, 3
        rep movsb
        pop edi
        pop esi
        ret 8
    zero:
        xor eax, eax
        pop esi
        ret 8
    }
}
