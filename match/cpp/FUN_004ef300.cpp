__declspec(naked) unsigned int FUN_004ef300()
{
    __asm {
        _emit 0x8b
        _emit 0x0d
        _emit 0x88
        _emit 0x76
        _emit 0x6a
        _emit 0x00
        mov eax, ecx
        add ecx, 0x10
        cmp eax, 0x029d1730
        _emit 0x89
        _emit 0x0d
        _emit 0x88
        _emit 0x76
        _emit 0x6a
        _emit 0x00
        jb short done
        xor eax, eax
    done:
        ret
    }
}
