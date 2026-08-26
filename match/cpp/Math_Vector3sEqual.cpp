struct Vec3s {
    short x;
    short y;
    short z;

    int Equal(const Vec3s& rhs);
};

int Vec3s::Equal(const Vec3s& rhs)
{
    return x == rhs.x && y == rhs.y && z == rhs.z;
}
