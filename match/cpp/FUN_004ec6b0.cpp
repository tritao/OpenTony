__declspec(naked) unsigned char FUN_004ec6b0()
{
    __asm {
        push esi
        mov esi, dword ptr [esp+8]
        xor al, al
        mov dl, byte ptr [esi]
        test dl, dl
        je short done
    next:
        and edx, 0xff
        test byte ptr [edx+ecx+4], 0x80
        je short skip
        mov al, 1
    skip:
        mov dl, byte ptr [esi+1]
        inc esi
        test dl, dl
        jne short next
    done:
        pop esi
        ret 4
    }
}
