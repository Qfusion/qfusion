#ifndef AI_VEC3_H
#define AI_VEC3_H

#include "../../gameshared/q_math.h"

class Vec3
{
    vec3_t vec;
public:
    explicit Vec3(const vec3_t that)
    {
        VectorCopy(that, data());
    }
    Vec3(const Vec3 &that)
    {
        VectorCopy(that.data(), data());
    }

    Vec3(vec_t x, vec_t y, vec_t z)
    {
        VectorSet(vec, x, y, z);
    }

    Vec3 &operator=(const Vec3 &that)
    {
        VectorCopy(that.data(), data());
        return *this;
    }

    float Length() const { return VectorLength(vec); }
    float LengthFast() const { return VectorLengthFast(vec); }
    float SquaredLength() const { return VectorLengthSquared(vec); }

    void Normalize() { VectorNormalize(vec); }
    void NormalizeFast() { VectorNormalizeFast(vec); }

    float *data() { return vec; }
    const float *data() const { return vec; }

    vec_t &x() { return vec[0]; }
    vec_t &y() { return vec[1]; }
    vec_t &z() { return vec[2]; }

    vec_t x() const { return vec[0]; }
    vec_t y() const { return vec[1]; }
    vec_t z() const { return vec[2]; }

    void operator+=(const Vec3 &that)
    {
        VectorAdd(vec, that.vec, vec);
    }
    void operator+=(const vec3_t that)
    {
        VectorAdd(vec, that, vec);
    }
    void operator-=(const Vec3 &that)
    {
        VectorSubtract(vec, that.vec, vec);
    }
    void operator-=(const vec3_t that)
    {
        VectorSubtract(vec, that, vec);
    }
    void operator*=(float scale)
    {
        VectorScale(vec, scale, vec);
    }
    Vec3 operator*(float scale) const
    {
        return Vec3(scale * x(), scale * y(), scale * z());
    }
    Vec3 operator+(const Vec3 &that) const
    {
        return Vec3(x() + that.x(), y() + that.y(), z() + that.z());
    }
    Vec3 operator+(const vec3_t that) const
    {
        return Vec3(x() + that[0], y() + that[1], z() + that[2]);
    }
    Vec3 operator-(const Vec3 &that) const
    {
        return Vec3(x() - that.x(), y() - that.y(), z() - that.z());
    }
    Vec3 operator-(const vec3_t that) const
    {
        return Vec3(x() - that[0], y() - that[1], z() - that[2]);
    }
    Vec3 operator-() const
    {
        return Vec3(-x(), -y(), -z());
    }

    vec_t Dot(const Vec3 &that) const
    {
        return _DotProduct(vec, that.vec);
    }
    float Dot(const vec3_t that) const
    {
        return _DotProduct(vec, that);
    }

    inline Vec3 Cross(const Vec3 &that) const
    {
        return Vec3(
            y() * that.z() - z() * that.y(),
            z() * that.x() - x() * that.z(),
            x() * that.y() - y() * that.x());
    }
    inline Vec3 Cross(const vec3_t that) const
    {
        return Vec3(
            y() * that[2] - z() * that[1],
            z() * that[0] - x() * that[2],
            x() * that[2] - y() * that[1]);
    }
};

inline Vec3 operator * (float scale, const Vec3 &v)
{
    return v * scale;
}

#endif
