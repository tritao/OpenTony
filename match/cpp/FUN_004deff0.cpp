void FUN_004deff0()
{
    int* first = *(int**)0x006a3a88;
    int zero = 0;
    *(unsigned char*)0x006a3b30 = 1;
    if (first != 0)
        *(int*)((char*)first + 0x2cc8) = zero;
    int* second = *(int**)0x006a18a8;
    *(int*)0x006a18bc = zero;
    if (second != 0) {
        *(int*)((char*)second + 0x2cc8) = zero;
        *(int*)((char*)*(int**)0x006a18a8 + 0x30b8) = zero;
    }
}
