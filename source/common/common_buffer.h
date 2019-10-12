#ifndef COMMON_BUFFER
#define COMMON_BUFFER

#include "common_types.h"

struct buffer {
   u64 size;
   u64 offset;
   u8 *data;
};

buffer Buffer(u64 size, u8 *data, u64 offset = 0) {
   buffer result = {};
   result.size = size;
   result.data = data;
   result.offset = offset;
   return result;
}

//Consume ---------------------------------
#define ConsumeStruct(b, struct) (struct *) ConsumeSize(b, sizeof(struct))
#define ConsumeArray(b, struct, length) (struct *) ConsumeSize(b, (length) * sizeof(struct))
u8 *ConsumeSize(buffer *b, u64 size) {
   Assert((b->offset + size) <= b->size);
   u8 *result = b->data + b->offset;
   b->offset += size;
   return result;
}

//Peek ------------------------------------
#define PeekStruct(b, struct) (struct *) PeekSize(b, sizeof(struct))
#define PeekArray(b, struct, length) (struct *) PeekSize(b, (length) * sizeof(struct))
u8 *PeekSize(buffer *b, u64 size) {
   Assert((b->offset + size) <= b->size);
   return b->data + b->offset;
}

//Write -----------------------------------
#define WriteStruct(b, struct_data) WriteSize(b, (u8 *)(struct_data), sizeof(*(struct_data)))
#define WriteArray(b, first_elem, length) WriteSize(b, (u8 *)(first_elem), (length) * sizeof(*(first_elem)))
u8 *PushSize(buffer *b, u64 size) {
   Assert((b->size - b->offset) >= size);
   u8 *result = b->data + b->offset;
   b->offset += size;
   return result;
}

u8 *WriteSize(buffer *b, void *in_data, u64 size) {
   u8 *data = (u8 *) in_data;
   u8 *result = PushSize(b, size);
   Copy(data, size, result);
   return result;
}

#define WriteStructData(b, type, name, code) do { type name = {}; code WriteStruct(b, &name); } while(false)

void Advance(buffer *b, u32 by) {
   Assert(by <= b->offset);
   //NOTE: this Copy is kinda sketchy because src & dst are in the same buffer
   Copy(b->data + by, b->offset - by, b->data);
   b->offset -= by;
}

//--------------- Things that depend on other common files ---------------
//Consume, Peek & Write for strings
#ifdef COMMON_STRINGS
   string ConsumeString(buffer *b, u32 length) {
      string result = {};
      result.length = length;
      result.text = ConsumeArray(b, char, length);
      return result;
   }

   string PeekString(buffer *b, u32 length) {
      string result = {};
      result.length = length;
      result.text = PeekArray(b, char, length);
      return result;
   }

   void WriteString(buffer *b, string str) {
      WriteArray(b, str.text, str.length);
   }
#endif

#ifdef COMMON_MEMORY
   //Creating buffers from arenas
   #define PushTempBuffer(size) PushBuffer(__temp_arena, (size))
   buffer PushBuffer(MemoryArena *arena, u64 size) {
      return Buffer(size, PushSize(arena, size));
   }
   
   //Copy on consume for arrays & strings
   #define ConsumeAndCopyArray(arena, b, struct, length) PushArrayCopy(arena, struct, ConsumeArray(b, struct, length), length)
   
   #ifdef COMMON_STRINGS
      string ConsumeAndCopyString(buffer *b, u32 length, MemoryArena *arena) {
         string result = {};
         result.length = length;
         result.text = ConsumeAndCopyArray(arena, b, char, length);
         return result;
      }
   #endif
#endif

#endif