void FUN_004da3e0(int first, int second)
{
    if (first != 0)
        *(int*)(first + 0x14) = second;
    if (second != 0)
        *(int*)(second + 4) = first;
}
