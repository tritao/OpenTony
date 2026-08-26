struct FlagObject {
    char padding[0xe4];
    unsigned char flag;

    int FUN_004ce290();
};

int FlagObject::FUN_004ce290()
{
    return flag != 0;
}
