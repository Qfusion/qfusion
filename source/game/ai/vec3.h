#ifndef AI_VEC3_H
#define AI_VEC3_H

#include "../../gameshared/q_math.h"

class Vec3
{
    vec3_t vec;
public:
    explicit Vec3(const vec3_t that)
    {
        VectorCopy(that, Data());
    }
    Vec3(const Vec3 &that)
    {
        VectorCopy(that.Data(), Data());
    }

    Vec3(vec_t x, vec_t y, vec_t z)
    {
        VectorSet(vec, x, y, z);
    }

    Vec3 &operator=(const Vec3 &that)
    {
        VectorCopy(that.Data(), Data());
        return *this;
    }

    float Length() const { return VectorLength(vec); }
    float LengthFast() const { return VectorLengthFast(vec); }
    float SquaredLength() const { return VectorLengthSquared(vec); }

    void Normalize() { VectorNormalize(vec); }
    void NormalizeFast() { VectorNormalizeFast(vec); }

    float *Data() { return vec; }
    const float *Data() const { return vec; }

    vec_t &X() { return vec[0]; }
    vec_t &Y() { return vec[1]; }
    vec_t &Z() { return vec[2]; }

    vec_t X() const { return vec[0]; }
    vec_t Y() const { return vec[1]; }
    vec_t Z() const { return vec[2]; }

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
        return Vec3(scale * X(), scale * Y(), scale * Z());
    }
    Vec3 operator+(const Vec3 &that) const
    {
        return Vec3(X() + that.X(), Y() + that.Y(), Z() + that.Z());
    }
    Vec3 operator+(const vec3_t that) const
    {
        return Vec3(X() + that[0], Y() + that[1], Z() + that[2]);
    }
    Vec3 operator-(const Vec3 &that) const
    {
        return Vec3(X() - that.X(), Y() - that.Y(), Z() - that.Z());
    }
    Vec3 operator-(const vec3_t that) const
    {
        return Vec3(X() - that[0], Y() - that[1], Z() - that[2]);
    }
    Vec3 operator-() const
    {
        return Vec3(-X(), -Y(), -Z());
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
            Y() * that.Z() - Z() * that.Y(),
            Z() * that.X() - X() * that.Z(),
            X() * that.Y() - Y() * that.X());
    }
    inline Vec3 Cross(const vec3_t that) const
    {
        return Vec3(
            Y() * that[2] - Z() * that[1],
            Z() * that[0] - X() * that[2],
            X() * that[2] - Y() * that[1]);
    }
};

inline Vec3 operator * (float scale, const Vec3 &v)
{
    return v * scale;
}

#endif
