struct Vec3s {
    short x;
    short y;
    short z;
};

Vec3s Math_Vector3sSubOut(const Vec3s& a, const Vec3s& b)
{
    Vec3s out;
    out.x = a.x - b.x;
    out.y = a.y - b.y;
    out.z = a.z - b.z;
    return out;
}
