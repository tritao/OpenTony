int FUN_004d0a40(unsigned int value)
{
    if (value == 0)
        return 0;
    int count = 0;
    while ((value & 1) == 0) {
        value >>= 1;
        ++count;
    }
    return count;
}
