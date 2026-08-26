// Import thunk for DirectDrawEnumerateExA.
__declspec(naked) void DirectDrawEnumerateExA()
{
    __asm {
        _emit 0xff
        _emit 0x25
        _emit 0x20
        _emit 0x80
        _emit 0x51
        _emit 0x00
    }
}
