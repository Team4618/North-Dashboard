#include "common_types.h"
#include "common_math.h"
#include "common_memory.h"
#include "common_strings.h"
#include "common_buffer.h"
#include "common_platform.h"

//NOTE: these are things that used to be in common.cpp but dont fit in any of the split up files
#define RIFF_CODE(str) ( ((str)[0] << 0) | ((str)[1] << 8) | ((str)[2] << 16) | ((str)[3] << 24) )

#include "stdlib.h"
f32 Random01() {
   return (f32)rand() / (f32)RAND_MAX;
}