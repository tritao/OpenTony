struct Vec3 {
    int x;
    int y;
    int z;

    Vec3& MulScalar(const int& scalar);
};

Vec3& Vec3::MulScalar(const int& scalar)
{
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
}
