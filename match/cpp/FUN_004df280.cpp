void FUN_004df280(int* object)
{
    int zero = 0;
    int count = 0x10;
    object[2] = zero;
    object[0] = zero;
    object[1] = zero;
    object += 4;
    do {
        object[-1] = zero;
        object[0] = zero;
        object += 2;
        --count;
    } while (count != 0);
}
