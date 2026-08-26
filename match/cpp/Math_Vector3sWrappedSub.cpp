struct Vec3s {
    short x;
    short y;
    short z;

    Vec3s WrappedSub(const Vec3s& rhs) const;
};

Vec3s Vec3s::WrappedSub(const Vec3s& rhs) const
{
    Vec3s out;
    out.x = x - rhs.x;
    out.y = y - rhs.y;
    out.z = z - rhs.z;
    if (out.x < -2048)
        out.x += 4096;
    if (out.x > 2048)
        out.x -= 4096;
    if (out.y < -2048)
        out.y += 4096;
    if (out.y > 2048)
        out.y -= 4096;
    if (out.z < -2048)
        out.z += 4096;
    if (out.z > 2048)
        out.z -= 4096;
    return out;
}
