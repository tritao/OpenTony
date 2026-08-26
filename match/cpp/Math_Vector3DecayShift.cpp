struct Vec3 {
    int x;
    int y;
    int z;

    Vec3& DecayShift(const unsigned char* counts);
};

Vec3& Vec3::DecayShift(const unsigned char* counts)
{
    x -= x >> counts[0];
    y -= y >> counts[1];
    z -= z >> counts[2];
    return *this;
}
