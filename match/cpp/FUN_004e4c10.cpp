typedef void (__stdcall *Callback)(int, int);

void FUN_004e4c10(int first, int second)
{
    (*((Callback*)0x0051823c))(first, second);
}
