int FUN_004d0a20(unsigned int value)
{
    int count = 0;
    while (value != 0) {
        count += value & 1;
        value >>= 1;
    }
    return count;
}
