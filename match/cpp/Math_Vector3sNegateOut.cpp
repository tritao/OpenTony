struct Vec3s {
    short x;
    short y;
    short z;

    Vec3s Negate() const;
};

Vec3s Vec3s::Negate() const
{
    Vec3s out;
    out.x = -x;
    out.y = -y;
    out.z = -z;
    return out;
}
