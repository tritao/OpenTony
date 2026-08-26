struct Vec3s {
    short x;
    short y;
    short z;

    Vec3s& DecayShift(const unsigned char* counts);
};

Vec3s& Vec3s::DecayShift(const unsigned char* counts)
{
    x -= x >> counts[0];
    y -= y >> counts[1];
    z -= z >> counts[2];
    return *this;
}
