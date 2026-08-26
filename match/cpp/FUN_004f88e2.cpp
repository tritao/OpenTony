// Import thunk for DirectInputCreateA.
__declspec(naked) void DirectInputCreateA()
{
    __asm {
        _emit 0xff
        _emit 0x25
        _emit 0x28
        _emit 0x80
        _emit 0x51
        _emit 0x00
    }
}
