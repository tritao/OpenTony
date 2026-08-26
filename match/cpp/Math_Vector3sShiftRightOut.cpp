struct Vec3s {
    short x;
    short y;
    short z;
};

Vec3s Math_Vector3sShiftRightOut(const Vec3s& value, const short& count)
{
    Vec3s out;
    out.x = value.x >> count;
    out.y = value.y >> count;
    out.z = value.z >> count;
    return out;
}
