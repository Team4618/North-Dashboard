Some little libraries that I've shared between many projects over the years (used to be lib/common.cpp)
Designed for unity builds (only one file gets compiled, everything is included in that file)

common_types - bit size types (uu8, s32, f32 ...) & some small utils (Assert, ArraySize, Clamp ...), Everything below depends on this
common_math - v2, v3, v4, mat4, rect2
common_memory - memory arenas & the temp arena
common_strings - length strings & related utils <optional dependance on common_memory>
common_buffer - buffers (usually used for ser/des to file formats) <optional dependance on common_memory, common_strings>
common_platform - file IO, platform memory allocation ... (depends on common_types, common_memory, common_strings & common_buffer)

just include common/common.h to use all of them