int FUN_004e78e0()
{
    for (int index = 0; index < 10; ++index) {
        if (((char*)0x006a6d10)[index] == 0)
            return index;
    }
    return -1;
}
