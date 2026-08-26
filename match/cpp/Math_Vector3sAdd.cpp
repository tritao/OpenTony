struct Vec3s {
    short x;
    short y;
    short z;

    Vec3s& Add(const Vec3s& rhs);
};

Vec3s& Vec3s::Add(const Vec3s& rhs)
{
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
}
