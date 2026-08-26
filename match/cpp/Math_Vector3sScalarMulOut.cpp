struct Vec3s {
    short x;
    short y;
    short z;
};

Vec3s Math_Vector3sScalarMulOut(const short& scalar, const Vec3s& value)
{
    Vec3s out;
    out.x = scalar * value.x;
    out.y = scalar * value.y;
    out.z = scalar * value.z;
    return out;
}
