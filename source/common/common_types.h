#ifndef COMMON_TYPES
#define COMMON_TYPES

#include "stdint.h"
typedef int8_t s8;
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;
typedef float f32;
#define F32_MAX 3.402823e+38
typedef double f64;

#define Assert(condition) if(!(condition)){*(char *)0 = 0;}
#define ArraySize(array) (sizeof(array) / sizeof((array)[0]))
#define Min(a, b) (((a) > (b)) ? (b) : (a))
#define Max(a, b) (((a) < (b)) ? (b) : (a))
#define Clamp(min, max, in) Min(Max(min, in), max)

#define ZeroStruct(var) _Zero((u8 *) (var), sizeof(*(var)))
void _Zero(u8 *data, u32 size) {
   for(u32 i = 0; i < size; i++) {
      data[i] = 0;
   }
}

void Copy(void *src_in, u32 size, void *dest_in) {
   u8 *src = (u8 *) src_in;
   u8 *dest = (u8 *) dest_in;
   for(u32 i = 0; i < size; i++) {
      dest[i] = src[i];
   }
}

// #define offsetof(st, m) ( (size_t)&(((st *)0)->m) )
#define ForEachArray(index, name, count, array, code) do { for(u32 index = 0; index < (count); index++){ auto name = (array) + index; code } } while(false)

#endif