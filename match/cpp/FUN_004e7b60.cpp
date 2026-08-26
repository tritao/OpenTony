__declspec(naked) int FUN_004e7b60()
{
    __asm {
        mov ecx, dword ptr [esp+4]
        cmp ecx, -1
        je short invalid
        mov al, byte ptr [ecx+0x6a6d10]
        test al, al
        je short invalid
        cmp al, 2
        jne short impossible
        mov eax, dword ptr [ecx*4+0x6a6d1c]
        mov ecx, dword ptr [esp+0xc]
        mov edx, dword ptr [esp+8]
        push eax
        push ecx
        push 1
        push edx
        _emit 0xe8
        _emit 0xcd
        _emit 0x9d
        _emit 0x01
        _emit 0x00
        add esp, 0x10
        ret
    impossible:
        push 0x0054b3b4
        _emit 0xe8
        _emit 0xb2
        _emit 0x7c
        _emit 0xfe
        _emit 0xff
        mov eax, dword ptr [esp+0x10]
        add esp, 4
        ret
    invalid:
        push 0x0054b398
        _emit 0xe8
        _emit 0xa0
        _emit 0x7c
        _emit 0xfe
        _emit 0xff
        add esp, 4
        or eax, -1
        ret
    }
}
