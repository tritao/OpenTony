struct Vec3 {
    int x;
    int y;
    int z;

    int NotEqual(const Vec3& rhs);
};

int Vec3::NotEqual(const Vec3& rhs)
{
    return x != rhs.x || y != rhs.y || z != rhs.z;
}
