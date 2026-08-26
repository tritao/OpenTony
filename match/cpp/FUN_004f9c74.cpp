// Import thunk for the fourth ordinal export.
__declspec(naked) void Ordinal_4()
{
    __asm {
        _emit 0xff
        _emit 0x25
        _emit 0x30
        _emit 0x80
        _emit 0x51
        _emit 0x00
    }
}
