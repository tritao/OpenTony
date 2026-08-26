struct Vec3 {
    int x;
    int y;
    int z;

    Vec3& ShiftLeft(const int& count);
};

Vec3& Vec3::ShiftLeft(const int& count)
{
    x <<= count;
    y <<= count;
    z <<= count;
    return *this;
}
