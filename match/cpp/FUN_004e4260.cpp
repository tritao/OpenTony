int FUN_004e4260()
{
    int count = 0;
    int offset = 0x6c;
    while (offset < 0x6c || *(int*)(0x006a42e8 + offset) == 0) {
        offset += 4;
        ++count;
        if (offset >= 0xec) {
            return -1;
        }
    }
    return count;
}
