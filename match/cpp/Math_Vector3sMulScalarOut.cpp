struct Vec3s {
    short x;
    short y;
    short z;
};

Vec3s Math_Vector3sMulScalarOut(const Vec3s& value, const short& scalar)
{
    Vec3s out;
    out.x = value.x * scalar;
    out.y = value.y * scalar;
    out.z = value.z * scalar;
    return out;
}
