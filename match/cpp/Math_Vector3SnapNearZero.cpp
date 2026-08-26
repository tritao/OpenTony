void __fastcall Math_Vector3sSnapNearZero(short* value)
{
    if (value[0] >= -1 && value[0] <= 1)
        value[0] = 0;
    if (value[1] >= -1 && value[1] <= 1)
        value[1] = 0;
    if (value[2] >= -1 && value[2] <= 1)
        value[2] = 0;
}
