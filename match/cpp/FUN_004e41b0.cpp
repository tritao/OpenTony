int FUN_004e41b0()
{
    __asm {
        mov eax, dword ptr [esp+4]
        and eax, 0xff
        mov al, byte ptr [eax+0x006a43e4]
        shr al, 7
    }
}
