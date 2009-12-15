#pragma once

#include <cmath>
struct Vec3 {
	float x,y,z;
	Vec3(float _x=0.0f, float _y=0.0f, float _z=0.0f) : x(_x), y(_y), z(_z) {};
	float mag() const { return std::sqrt(x*x+y*y+z*z); }
	Vec3 norm() const { float r = 1.0/mag(); return Vec3( x*r , y*r, z*r); }
};

// vector ops
inline Vec3 operator + (const Vec3& l, const Vec3& r){ return Vec3( l.x+r.x , l.y+l.y, l.z+l.z); }
inline Vec3 operator - (const Vec3& l, const Vec3& r){ return Vec3( l.x-r.x , l.y-l.y, l.z-l.z); }

// vector scalar ops
inline Vec3 operator + (const Vec3& l, float r){ return Vec3( l.x+r , l.y+r, l.z+r); }
inline Vec3 operator - (const Vec3& l, float r){ return Vec3( l.x-r , l.y-r, l.z-r); }
inline Vec3 operator * (const Vec3& l, float r){ return Vec3( l.x*r , l.y*r, l.z*r); }
inline Vec3 operator / (const Vec3& l, float r){ return Vec3( l.x/r , l.y/r, l.z/r); }


