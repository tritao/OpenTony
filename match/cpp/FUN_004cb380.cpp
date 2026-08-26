void FUN_004cb380(int* first, int* second)
{
    *first = *(int*)(0x0056e340 + *(int*)0x0056e338 * 8);
    *second = *(int*)(0x0056e344 + *(int*)0x0056e338 * 8);
    int next = *(int*)0x0056e338 + 1;
    *(int*)0x0056e338 = next % 30;
}
