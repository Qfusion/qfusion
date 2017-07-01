#ifndef AI_VEC3_H
#define AI_VEC3_H

#include "../../gameshared/q_math.h"

class Vec3
{
	vec3_t vec;

public:
	explicit Vec3( const vec3_t that ) {
		VectorCopy( that, Data() );
	}
	Vec3( const Vec3 &that ) {
		VectorCopy( that.Data(), Data() );
	}

	Vec3( vec_t x, vec_t y, vec_t z ) {
		VectorSet( vec, x, y, z );
	}

	Vec3 &operator=( const Vec3 &that ) {
		VectorCopy( that.Data(), Data() );
		return *this;
	}

	float Length() const { return (float)VectorLength( vec ); }
	float LengthFast() const { return VectorLengthFast( vec ); }
	float SquaredLength() const { return VectorLengthSquared( vec ); }

	float DistanceTo( const Vec3 &that ) const { return DistanceTo( that.Data() ); }
	float DistanceTo( const vec3_t that ) const { return (float)Distance( vec, that ); }
	float FastDistanceTo( const Vec3 &that ) const { return FastDistanceTo( that.Data() ); }
	float FastDistanceTo( const vec3_t that ) const { return DistanceFast( vec, that ); }
	float SquareDistanceTo( const Vec3 &that ) const { return SquareDistanceTo( that.Data() ); }
	float SquareDistanceTo( const vec3_t that ) const { return DistanceSquared( vec, that ); }
	float Distance2DTo( const Vec3 &that ) const { return Distance2DTo( that.vec ); }
	float Distance2DTo( const vec3_t that ) const { return sqrtf( SquareDistance2DTo( that ) ); }
	float FastDistance2DTo( const Vec3 &that ) const { return FastDistance2DTo( that.vec ); }
	float FastDistance2DTo( const vec3_t that ) const { return 1.0f / Q_RSqrt( SquareDistance2DTo( that ) ); }
	float SquareDistance2DTo( const Vec3 &that ) const { return SquareDistanceTo( that.vec ); }
	float SquareDistance2DTo( const vec3_t that ) const {
		float dx = vec[0] - that[0];
		float dy = vec[1] - that[1];
		return dx * dx + dy * dy;
	}

	float Normalize() {
		float squareLength = VectorLengthSquared( vec );
		if( squareLength > 0 ) {
			float invLength = 1.0f / sqrtf( squareLength );
			VectorScale( vec, invLength, vec );
			return 1.0f / invLength;
		}
		return 0.0f;
	}
	float NormalizeFast() {
		float invLength = Q_RSqrt( VectorLengthSquared( vec ) );
		VectorScale( vec, invLength, vec );
		return 1.0f / invLength;
	}

	float *Data() { return vec; }
	const float *Data() const { return vec; }

	void Set( const Vec3 &that ) { Set( that.Data() ); }
	void Set( const vec3_t that ) { Set( that[0], that[1], that[2] ); }
	void Set( vec_t x, vec_t y, vec_t z ) {
		VectorSet( this->vec, x, y, z );
	}
	void CopyTo( Vec3 &that ) const { that.Set( *this ); }
	void CopyTo( vec3_t that ) const { VectorCopy( this->vec, that ); }
	void CopyTo( vec_t *x, vec_t *y, vec_t *z ) const {
		if( x ) {
			*x = X();
		}
		if( y ) {
			*y = Y();
		}
		if( z ) {
			*z = Z();
		}
	}

	vec_t &X() { return vec[0]; }
	vec_t &Y() { return vec[1]; }
	vec_t &Z() { return vec[2]; }

	vec_t X() const { return vec[0]; }
	vec_t Y() const { return vec[1]; }
	vec_t Z() const { return vec[2]; }

	void operator+=( const Vec3 &that ) {
		VectorAdd( vec, that.vec, vec );
	}
	void operator+=( const vec3_t that ) {
		VectorAdd( vec, that, vec );
	}
	void operator-=( const Vec3 &that ) {
		VectorSubtract( vec, that.vec, vec );
	}
	void operator-=( const vec3_t that ) {
		VectorSubtract( vec, that, vec );
	}
	void operator*=( float scale ) {
		VectorScale( vec, scale, vec );
	}
	Vec3 operator*( float scale ) const {
		return Vec3( scale * X(), scale * Y(), scale * Z() );
	}
	Vec3 operator+( const Vec3 &that ) const {
		return Vec3( X() + that.X(), Y() + that.Y(), Z() + that.Z() );
	}
	Vec3 operator+( const vec3_t that ) const {
		return Vec3( X() + that[0], Y() + that[1], Z() + that[2] );
	}
	Vec3 operator-( const Vec3 &that ) const {
		return Vec3( X() - that.X(), Y() - that.Y(), Z() - that.Z() );
	}
	Vec3 operator-( const vec3_t that ) const {
		return Vec3( X() - that[0], Y() - that[1], Z() - that[2] );
	}
	Vec3 operator-() const {
		return Vec3( -X(), -Y(), -Z() );
	}

	vec_t Dot( const Vec3 &that ) const {
		return _DotProduct( vec, that.vec );
	}
	float Dot( const vec3_t that ) const {
		return _DotProduct( vec, that );
	}

	inline Vec3 Cross( const Vec3 &that ) const {
		return Vec3(
			Y() * that.Z() - Z() * that.Y(),
			Z() * that.X() - X() * that.Z(),
			X() * that.Y() - Y() * that.X() );
	}
	inline Vec3 Cross( const vec3_t that ) const {
		return Vec3(
			Y() * that[2] - Z() * that[1],
			Z() * that[0] - X() * that[2],
			X() * that[2] - Y() * that[1] );
	}
};

inline Vec3 operator *( float scale, const Vec3 &v ) {
	return v * scale;
}

#endif
