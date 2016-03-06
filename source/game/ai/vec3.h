#ifndef AI_VEC3_H
#define AI_VEC3_H

#include "../../gameshared/q_math.h"

class Vec3;

class Vec3Like
{
    friend class Vec3Ref;
    friend class Vec3;
private:
    explicit Vec3Like(float *data): dataPtr(data) {}
    float *dataPtr;
public:

    float *data() { return dataPtr; }
    const float *data() const { return dataPtr; }

    float &x() { return dataPtr[0]; }
    float &y() { return dataPtr[1]; }
    float &z() { return dataPtr[2]; }

    float x() const { return dataPtr[0]; }
    float y() const { return dataPtr[1]; }
    float z() const { return dataPtr[2]; }

    float Length() const { return VectorLength(dataPtr); }
    float LengthFast() const { return VectorLengthFast(dataPtr); }
    float SquaredLength() const { return VectorLengthSquared(dataPtr); }

    void Normalize() { VectorNormalize(dataPtr); }
    void NormalizeFast() { VectorNormalizeFast(dataPtr); }

    void operator += (const Vec3Like &that)
    {
        x() += that.x();
        y() += that.y();
        z() += that.z();
    }
    void operator += (const vec3_t that) { *this += Vec3Like(const_cast<float *>(that)); }

    void operator -= (const Vec3Like &that)
    {
        x() -= that.x();
        y() -= that.y();
        z() -= that.z();
    }
    void operator -= (const vec3_t that) { *this -= Vec3Like(const_cast<float *>(that)); }

    void operator *= (float scale)
    {
        x() *= scale;
        y() *= scale;
        z() *= scale;
    }

    // Can't implement these methods here since Vec3 is not defined yet
    Vec3 operator * (float scale) const;
    Vec3 operator + (const Vec3Like &that) const;
    Vec3 operator + (const vec3_t that) const;
    Vec3 operator - (const Vec3Like &that) const;
    Vec3 operator - (const vec3_t that) const;
    Vec3 operator - () const;

    float Dot(const Vec3Like &that) const
    {
        return x() * that.x() + y() * that.y() + z() * that.z();
    }
    float Dot(const vec3_t &that) const
    {
        return x() * that[0] + y() * that[1] + z() * that[2];
    }

    Vec3 Cross(const Vec3Like &that) const;
    Vec3 Cross(const vec3_t that) const;
};

class Vec3Ref: public Vec3Like
{
public:
    explicit Vec3Ref(vec3_t that): Vec3Like(that) {}
};

class Vec3: public Vec3Like
{
public:
    vec3_t vec;

    explicit Vec3(const vec3_t that): Vec3Like(vec)
    {
        VectorCopy(that, vec);
    }
    explicit Vec3(const Vec3Like &that): Vec3Like(vec)
    {
        VectorCopy(that.data(), vec);
    }

    Vec3(float x, float y, float z): Vec3Like(vec)
    {
        this->x() = x;
        this->y() = y;
        this->z() = z;
    }
    Vec3 &operator=(const Vec3Like &that)
    {
        VectorCopy(that.data(), vec);
        return *this;
    }
};

inline Vec3 operator * (float scale, const Vec3Like &v)
{
    return v * scale;
}

inline Vec3 Vec3Like::operator * (float scale) const
{
    return Vec3(scale * x(), scale * y(), scale * z());
}
inline Vec3 Vec3Like::operator + (const Vec3Like &that) const
{
    return Vec3(x() + that.x(), y() + that.y(), z() + that.z());
}
inline Vec3 Vec3Like::operator + (const vec3_t that) const
{
    return Vec3(x() + that[0], y() + that[1], z() + that[2]);
}
inline Vec3 Vec3Like::operator - (const Vec3Like &that) const
{
    return Vec3(x() - that.x(), y() - that.y(), z() - that.z());
}
inline Vec3 Vec3Like::operator - (const vec3_t that) const
{
    return Vec3(x() - that[0], y() - that[1], z() - that[2]);
}
inline Vec3 Vec3Like::operator - () const
{
    return Vec3(-x(), -y(), -z());
}

inline Vec3 Vec3Like::Cross(const Vec3Like &that) const
{
    return Vec3(
        y() * that.z() - z() * that.y(),
        z() * that.x() - x() * that.z(),
        x() * that.y() - y() * that.x());
}
inline Vec3 Vec3Like::Cross(const vec3_t that) const
{
    return Vec3(
        y() * that[2] - z() * that[1],
        z() * that[0] - x() * that[2],
        x() * that[2] - y() * that[1]);
}

#endif
