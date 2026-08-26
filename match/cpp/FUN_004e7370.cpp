// Stops the current music stream, waiting for the worker to acknowledge it.
__declspec(naked) void FUN_004e7370()
{
    __asm {
        _emit 0xa0
        _emit 0xe0
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        push ebx
        xor ebx, ebx
        cmp al, bl
        je short done
        _emit 0x38
        _emit 0x1d
        _emit 0xe2
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        jne short stop_now
        push esi
        _emit 0x8b
        _emit 0x35
        _emit 0x7c
        _emit 0x80
        _emit 0x51
        _emit 0x00
        _emit 0xc6
        _emit 0x05
        _emit 0xa8
        _emit 0xb0
        _emit 0x54
        _emit 0x00
        _emit 0x01
    wait_for_worker:
        push 10
        call esi
        _emit 0x38
        _emit 0x1d
        _emit 0xe0
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        jne short wait_for_worker
        pop esi
        push 0x0054b298
        // Log "Stopping Music..." at 0x004cf850.
        _emit 0xe8
        _emit 0xa7
        _emit 0x84
        _emit 0xfe
        _emit 0xff
        add esp, 4
        pop ebx
        ret
    stop_now:
        // Tear down the stream at 0x004e6da0.
        _emit 0xe8
        _emit 0xed
        _emit 0xf9
        _emit 0xff
        _emit 0xff
        push 0x0054b298
        _emit 0x88
        _emit 0x1d
        _emit 0xe2
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        _emit 0x88
        _emit 0x1d
        _emit 0xe0
        _emit 0x6c
        _emit 0x6a
        _emit 0x00
        _emit 0x88
        _emit 0x1d
        _emit 0xa8
        _emit 0xb0
        _emit 0x54
        _emit 0x00
        _emit 0x89
        _emit 0x1d
        _emit 0x08
        _emit 0x6d
        _emit 0x6a
        _emit 0x00
        // Log the completed stop at 0x004cf850.
        _emit 0xe8
        _emit 0x7b
        _emit 0x84
        _emit 0xfe
        _emit 0xff
        add esp, 4
    done:
        pop ebx
        ret
    }
}
