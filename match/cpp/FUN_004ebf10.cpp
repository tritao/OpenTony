__declspec(naked) void* FUN_004ebf10()
{
    __asm {
        mov edx, ecx
        push esi
        xor al, al
        push edi
        mov dword ptr [edx], 0x00519a18
        mov byte ptr [edx+0x245c], al
        mov byte ptr [edx+0x245d], al
        mov dword ptr [edx+0x2458], -1
        lea edi, [edx+0x18]
        mov esi, 0x10
    loop_start:
        mov eax, 0x244
        xor ecx, ecx
        add edi, eax
        dec esi
        jne short loop_start
        pop edi
        mov eax, edx
        pop esi
        ret
    }
}
