struct Vec3 {
    int x;
    int y;
    int z;

    Vec3& ShiftRight(const int& count);
};

Vec3& Vec3::ShiftRight(const int& count)
{
    x >>= count;
    y >>= count;
    z >>= count;
    return *this;
}
