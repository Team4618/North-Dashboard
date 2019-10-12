#ifndef COMMON_MATH
#define COMMON_MATH

#include "common_types.h"

#define PI32 3.141592653589793238462

f32 lerp(f32 a, f32 t, f32 b) {
   return a + t * (b - a);
}

f32 abs(f32 x) {
   return (x > 0) ? x : -x;
}

f64 abs(f64 x) {
   return (x > 0) ? x : -x;
}

f32 ToDegrees(f32 r) {
   return r * (180 / PI32);
}

f32 ToRadians(f32 d) {
   return d * (PI32 / 180);
}

f32 AngleBetween(f32 angle1, f32 angle2, bool clockwise) {
   if(angle2 > angle1) {
      return clockwise ? (angle2 - angle1 - 360) : (angle2 - angle1);
   } else {
      return clockwise ? (angle2 - angle1) : (angle2 - angle1 + 360);
   }
}

bool IsClockwiseShorter(f32 angle1, f32 angle2) {
   if(angle2 > angle1) {
      return abs(angle2 - angle1 - 360) < abs(angle2 - angle1);
   } else {
      return abs(angle2 - angle1) < abs(angle2 - angle1 + 360);
   }
}

f32 CanonicalizeAngle_Degrees(f32 rawAngle) {
   s32 revolutions = (s32) (rawAngle / 360);
   f32 modPI = (rawAngle - revolutions * 360);
   return modPI < 0 ? 360 + modPI : modPI;
}

f32 CanonicalizeAngle_Radians(f32 rawAngle) {
   s32 revolutions = (s32) (rawAngle / (2 * PI32));
   f32 modPI = (rawAngle - revolutions * (2 * PI32));
   return modPI < 0 ? (2 * PI32) + modPI : modPI;
}

f32 AngleBetween_Radians(f32 angle1, f32 angle2, bool clockwise) {
   if(angle2 > angle1) {
      return clockwise ? (angle2 - angle1 - 2*PI32) : (angle2 - angle1);
   } else {
      return clockwise ? (angle2 - angle1) : (angle2 - angle1 + 2*PI32);
   }
}

bool IsClockwiseShorter_Radians(f32 angle1, f32 angle2) {
   if(angle2 > angle1) {
      return abs(angle2 - angle1 - 2*PI32) < abs(angle2 - angle1);
   } else {
      return abs(angle2 - angle1) < abs(angle2 - angle1 + 2*PI32);
   }
}

f32 ShortestAngleBetween_Radians(f32 a, f32 b) {
   return AngleBetween_Radians(a, b, IsClockwiseShorter_Radians(a, b));
}

union v4 {
   struct { f32 r, g, b, a; };
   struct { f32 x, y, z, w; };
   f32 e[4];
};

inline v4 V4(f32 x, f32 y, f32 z, f32 w) {
   v4 result = {x, y, z, w}; 
   return result;
}

v4 operator* (f32 s, v4 v) {
	v4 output = {};
	output.x = v.x * s;
	output.y = v.y * s;
	output.z = v.z * s;
	output.w = v.w * s;
   return output;
}

bool operator==(v4 a, v4 b) {
   return (a.x == b.x) && 
          (a.y == b.y) && 
          (a.z == b.z) && 
          (a.w == b.w);
}

v4 lerp(v4 a, f32 t, v4 b) {
   return V4(lerp(a.r, t, b.r),
             lerp(a.g, t, b.g),
             lerp(a.b, t, b.b), 
             lerp(a.a, t, b.a));
}

u32 PackColourRGBA(v4 rgba) {
   return ((u32)(0xFF * rgba.r) << 0) | 
          ((u32)(0xFF * rgba.g) << 8) |
          ((u32)(0xFF * rgba.b) << 16) |
          ((u32)(0xFF * rgba.a) << 24);
}

union v2 {
   struct { f32 u, v; };
   struct { f32 x, y; };
   f32 e[2];
};

inline v2 V2(f32 x, f32 y) {
   v2 result = {x, y};
   return result;
}

inline v2 XOf(v2 v) {
   v2 result = {v.x, 0};
   return result;
}

inline v2 YOf(v2 v) {
   v2 result = {0, v.y};
   return result;
}

v2 operator+ (v2 a, v2 b) {
	v2 output = {};
	output.x = a.x + b.x;
	output.y = a.y + b.y;
	return output;
}

v2 operator- (v2 v) {
	return V2(-v.x, -v.y);
}

v2 operator- (v2 a, v2 b) {
	v2 output = {};
	output.x = a.x - b.x;
	output.y = a.y - b.y;
	return output;
}

v2 operator* (v2 v, f32 s) {
	v2 output = {};
	output.x = v.x * s;
	output.y = v.y * s;
	return output;
}

v2 operator* (f32 s, v2 v) {
	v2 output = {};
	output.x = v.x * s;
	output.y = v.y * s;
	return output;
}

v2 operator/ (v2 v, f32 s) {
	v2 output = {};
	output.x = v.x / s;
	output.y = v.y / s;
	return output;
}

v2 operator/ (v2 a, v2 b) {
	v2 output = {};
	output.x = a.x / b.x;
	output.y = a.y / b.y;
	return output;
}

bool operator==(v2 a, v2 b) {
   return (a.x == b.x) && 
          (a.y == b.y);
}

bool operator!=(v2 a, v2 b) {
   return !(a == b);
}

v2 Perp(v2 v) {
   return V2(v.y, -v.x);
}

v2 lerp(v2 a, f32 t, v2 b) {
   return V2(lerp(a.x, t, b.x), lerp(a.y, t, b.y));
}

#include "math.h"

u32 Power(u32 base, u32 exp) {
   u32 result = 1;
   while(exp) {
      if (exp & 1)
         result *= base;

      exp /= 2;
      base *= base;
   }

   return result;
}

f32 Sign(f32 x) {
   return x > 0 ? 1 : -1;
}

f32 Length(v2 a) { return sqrtf(a.x * a.x + a.y * a.y); }
f32 Distance(v2 a, v2 b) { return Length(a - b); }

v2 Normalize(v2 v) {
   f32 len = Length(v);
   return (len == 0) ? V2(0, 0) : (v / len);
}

v2 AspectRatio(v2 initial_size, v2 size_to_fit) {
   f32 rs = size_to_fit.x / size_to_fit.y;
   f32 ri = initial_size.x / initial_size.y; 
   return rs > ri ? V2(initial_size.x * size_to_fit.y / initial_size.y, size_to_fit.y) : 
                    V2(size_to_fit.x, initial_size.y * size_to_fit.x / initial_size.x);
}

f32 Dot(v2 a, v2 b) {
   return a.x * b.x + a.y * b.y;
}

v2 Midpoint(v2 a, v2 b) {
   return (a + b) / 2;
}

f32 DistFromLine(v2 a, v2 b, v2 p) {
   f32 num = (a.y - b.y) * p.x - (a.x - b.x) * p.y + a.x*b.y - a.y*b.x;
   return abs(num) / sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

v2 ClosestPointOnLine(v2 a, v2 b, v2 p) {
   v2 line_p0 = a;
   v2 line_d = b - a;
   f32 t = (Dot(p, line_d) - Dot(line_p0, line_d)) / Dot(line_d, line_d);
   return line_p0 + t * line_d;
}

v2 CubicHermiteSpline(v2 a_pos, v2 a_tan, v2 b_pos, v2 b_tan, f32 t) {
   return (2*t*t*t - 3*t*t + 1)*a_pos + (t*t*t - 2*t*t + t)*a_tan + 
          (-2*t*t*t + 3*t*t)*b_pos + (t*t*t - t*t)*b_tan;
}

v2 CubicHermiteSplineTangent(v2 a_pos, v2 a_tan, v2 b_pos, v2 b_tan, f32 t) {
   return 6*t*(t - 1)*a_pos + 
          (3*t*t - 4*t + 1)*a_tan + 
          (6*t - 6*t*t)*b_pos + 
          (3*t*t - 2*t)*b_tan;
}

union v3 {
   struct { f32 r, g, b; };
   struct { f32 x, y, z; };
   f32 e[3];
};

v3 V3(f32 x, f32 y, f32 z) {
   v3 result = {x, y, z};
   return result;
}

v3 operator+ (v3 a, v3 b) {
	v3 output = {};
	output.x = a.x + b.x;
	output.y = a.y + b.y;
   output.z = a.z + b.z;
	return output;
}

v3 operator/ (v3 v, f32 s) {
	v3 output = {};
	output.x = v.x / s;
	output.y = v.y / s;
   output.z = v.z / s;
	return output;
}

v3 operator* (v3 v, f32 s) {
	v3 output = {};
	output.x = v.x * s;
	output.y = v.y * s;
   output.z = v.z * s;
	return output;
}

v3 operator* (f32 s, v3 v) {
   return v * s;
}

f32 Length(v3 a) { 
   return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z); 
}

v3 Normalize(v3 v) {
   f32 len = Length(v);
   return (len == 0) ? V3(0, 0, 0) : (v / len);
}

struct rect2 {
   v2 min;
   v2 max;
};

rect2 operator+ (v2 offset, rect2 r) {
	rect2 result = r;
	result.min = result.min + offset;
	result.max = result.max + offset;
	return result;
}

bool operator== (rect2 a, rect2 b) {
   return (a.min == b.min) && (a.max == b.max);
}

bool operator!= (rect2 a, rect2 b) {
   return (a.min != b.min) || (a.max != b.max);
}

inline rect2 RectMinSize(v2 min, v2 size) {
   rect2 result = {};
   
   result.min = min;
   result.max = min + size;
   
   return result;
}

inline rect2 RectMinMax(v2 min, v2 max) {
   rect2 result = {};
   
   result.min = min;
   result.max = max;
   
   return result;
}

inline rect2 RectCenterSize(v2 pos, v2 size) {
   rect2 result = {};
   
   result.min = pos - 0.5 * size;
   result.max = pos + 0.5 * size;
   
   return result;
}

inline v2 Size(rect2 r) {
   return r.max - r.min;
}

inline v2 Center(rect2 r) {
   return (r.min + r.max) / 2;
}

inline v2 ClampTo(v2 p, rect2 r) {
   return V2(Clamp(r.min.x, r.max.x, p.x), 
             Clamp(r.min.y, r.max.y, p.y));
}

inline bool Contains(rect2 rect, v2 vec) {
   bool result = (vec.x >= rect.min.x) && 
                 (vec.x <= rect.max.x) &&
                 (vec.y >= rect.min.y) &&
                 (vec.y <= rect.max.y);
                
   return result;
}

inline bool IsInside(rect2 a, rect2 b) {
   return Contains(b, a.min) && Contains(b, a.max);
}

inline rect2 Overlap(rect2 a, rect2 b) {
   rect2 result = {};
   
   result.min.x = Max(a.min.x, b.min.x);
   result.min.y = Max(a.min.y, b.min.y);
   result.max.x = Min(a.max.x, b.max.x);
   result.max.y = Min(a.max.y, b.max.y);
   
   if((result.min.x > result.max.x) || (result.min.y > result.max.y))
      return RectCenterSize(V2(0,0), V2(0, 0));
   
   return result;
}

inline rect2 Union(rect2 a, rect2 b) {
   rect2 result = {};
   
   result.min.x = Min(a.min.x, b.min.x);
   result.min.y = Min(a.min.y, b.min.y);
   result.max.x = Max(a.max.x, b.max.x);
   result.max.y = Max(a.max.y, b.max.y);
   
   return result;
}

union mat4 {
   f32 e[16];
};

f32 &M(mat4 &m, u32 i, u32 j) {
   return m.e[i + 4*j];
}

mat4 operator* (mat4 A, mat4 B) {
	mat4 output = {};

   for(u32 i = 0; i < 4; i++) {
      for(u32 j = 0; j < 4; j++) {
         M(output, i, j) = M(A, i, 0) * M(B, 0, j) + 
                           M(A, i, 1) * M(B, 1, j) +
                           M(A, i, 2) * M(B, 2, j) +
                           M(A, i, 3) * M(B, 3, j);  
      }
   }

	return output;
}

mat4 Identity() {
   mat4 A = {};
   M(A, 0, 0) = 1;
   M(A, 1, 1) = 1;
   M(A, 2, 2) = 1;
   M(A, 3, 3) = 1;
   return A;
}

mat4 Orthographic(f32 top, f32 bottom, f32 left, f32 right, f32 nearPlane, f32 far) {
   mat4 A = {};
   
   M(A, 0, 0) = 2 / (right - left);
   M(A, 1, 1) = 2 / (top - bottom);
   M(A, 2, 2) = -2 / (far - nearPlane);

   M(A, 0, 3) = -(right + left) / (right - left);
   M(A, 1, 3) = -(top + bottom) / (top - bottom);
   M(A, 2, 3) = -(far + nearPlane) / (far - nearPlane);
   M(A, 3, 3) = 1;
   
   return A;
}

mat4 Perspective(f32 fovY, f32 aspect, f32 n, f32 f) {
   mat4 A = {};

   f32 tangent = tanf(fovY / 2);
   f32 height = n * tangent;
   f32 width = height * aspect; 

   f32 t = height;
   f32 b = -height;
   f32 r = width;
   f32 l = -width;
   
   M(A, 0, 0) = 2*n/(r - l);
   M(A, 1, 1) = 2*n/(t - b);

   M(A, 0, 2) = (r + l)/(r - l);
   M(A, 1, 2) = (t + b)/(t - b);
   M(A, 2, 2) = -(f + n)/(f - n);
   M(A, 3, 2) = -1;

   M(A, 2, 3) = -2*f*n/(f - n);
   
   return A;
}

mat4 Translation(v3 t) {
   mat4 A = Identity();
   M(A, 0, 3) = t.x;
   M(A, 1, 3) = t.y;
   M(A, 2, 3) = t.z;
   return A;
}

mat4 Scale(v3 s) {
   mat4 A = {};
   M(A, 0, 0) = s.x;
   M(A, 1, 1) = s.y;
   M(A, 2, 2) = s.z;
   M(A, 3, 3) = 1;
   return A;
}

mat4 Rotation(v3 u, f32 t) {
   mat4 A = {};

   f32 c = cosf(t);
   f32 s = sinf(t);

   M(A, 0, 0) = u.x*u.x*(1-c) + c;
   M(A, 1, 0) = u.x*u.y*(1-c) + u.z*s;
   M(A, 2, 0) = u.x*u.z*(1-c) - u.y*s;

   M(A, 0, 1) = u.x*u.y*(1-c) - u.z*s;
   M(A, 1, 1) = u.y*u.y*(1-c) + c;
   M(A, 2, 1) = u.y*u.z*(1-c) + u.x*s;

   M(A, 0, 2) = u.x*u.z*(1-c) + u.y*s;
   M(A, 1, 2) = u.y*u.z*(1-c) - u.x*s;
   M(A, 2, 2) = u.z*u.z*(1-c) + c;

   M(A, 3, 3) = 1;

   return A;
}

#endif