struct Vec3 {
    int x;
    int y;
    int z;

    Vec3& Add(const Vec3& rhs);
};

Vec3& Vec3::Add(const Vec3& rhs)
{
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
}
