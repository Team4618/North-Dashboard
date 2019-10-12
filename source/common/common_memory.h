#ifndef COMMON_MEMORY
#define COMMON_MEMORY

#include "common_types.h"

#define Kilobyte(BYTE) BYTE * 1024
#define Megabyte(BYTE) Kilobyte(BYTE) * 1024
#define Gigabyte(BYTE) Megabyte(BYTE) * 1024

//Memory Arena ----------------------------
struct MemoryArenaBlock {
   MemoryArenaBlock *next;
   
   u64 size;
   u64 used;
   u8 *memory;
};

typedef MemoryArenaBlock *(*alloc_arena_block_callback)(u64 size);
struct MemoryArena {
   u64 initial_size;
   alloc_arena_block_callback alloc_block;

   bool valid;
   MemoryArena *parent;
   MemoryArenaBlock *latest_block;
   u64 latest_used;
   
   MemoryArenaBlock *first_block;
   MemoryArenaBlock *curr_block;
};

u8 *PushSize(MemoryArena *arena, u64 size, bool assert_on_empty = true) {
   Assert(arena->valid);

   MemoryArenaBlock *curr_block = arena->curr_block;
   if(curr_block->size < (size + curr_block->used)) {
      if(curr_block->next == NULL) {
         if(arena->alloc_block == NULL) {
            if(assert_on_empty)
               Assert(false);

            return NULL;
         } else {
            //TODO: what size should we allocate?
            MemoryArenaBlock *new_block = arena->alloc_block(Max(arena->initial_size, size));
            
            curr_block->next = new_block;
            arena->curr_block = new_block;
         }
      } else {
         arena->curr_block = curr_block->next; 
      }
      curr_block = arena->curr_block;
   }

   Assert(curr_block->size >= (curr_block->used + size));
   u8 *result = curr_block->memory + curr_block->used;
   curr_block->used += size;
   _Zero(result, size);

   return result;
}

bool CanAllocate(MemoryArena *arena, u64 size) {
   if(arena->alloc_block != NULL)
      return true;

   for(MemoryArenaBlock *block = arena->curr_block; 
       block; block = block->next)
   {
      if(block->size >= (size + block->used))
         return true;
   }

   return false;
}

#define PushStruct(arena, struct) (struct *) PushSize(arena, sizeof(struct))
#define PushArray(arena, struct, length) (struct *) PushSize(arena, (length) * sizeof(struct))
#define PushArrayCopy(arena, struct, first_elem, length) (struct *) PushCopy(arena, first_elem, length * sizeof(struct))

//TODO: ability to create an arena from a given block of memory (dont allow it to expand)

MemoryArena *PushArena(MemoryArena *arena, u64 size) {
   MemoryArenaBlock *block = PushStruct(arena, MemoryArenaBlock);
   block->size = size;
   block->memory = PushSize(arena, size);
   
   MemoryArena *result = PushStruct(arena, MemoryArena);
   result->initial_size = size;
   result->first_block = block;
   result->curr_block = block;
   result->valid = true;
   
   return result;
}

u8 *PushCopy(MemoryArena *arena, void *src, u32 size) {
   u8 *result = PushSize(arena, size);
   Copy(src, size, result);
   return result;
}

void Reset(MemoryArena *arena) {
   for(MemoryArenaBlock *curr_block = arena->first_block;
       curr_block; curr_block = curr_block->next)
   {
      curr_block->used = 0;
   }
   
   arena->curr_block = arena->first_block;
}

//Arena lock & rollback ------------------- 
MemoryArena BeginTemp(MemoryArena *arena) {
   MemoryArena result = {};
   result.initial_size = arena->initial_size;
   result.first_block = arena->first_block;
   result.curr_block = arena->curr_block;

   result.valid = true;
   result.parent = arena;
   result.latest_block = arena->curr_block;
   result.latest_used = arena->curr_block->used;

   arena->valid = false;
   return result;
}

void EndTemp(MemoryArena *temp) {
   temp->latest_block->used = temp->latest_used;
   for(MemoryArenaBlock *block = temp->latest_block->next;
       block; block = block->next)
   {
      block->used = 0;
   } 

   temp->valid = false;
   temp->parent->curr_block = temp->latest_block;
   temp->parent->valid = true;
}

//Temp Arena ------------------------------
//TODO: how do we do per-thread temp arenas?
MemoryArena *__temp_arena = NULL;

struct TempArena {
   MemoryArena arena;

   TempArena(MemoryArena *_arena = __temp_arena) {
      arena = BeginTemp(_arena);
   } 

   ~TempArena() {
      EndTemp(&arena);
   }
};

#define PushTempSize(size) PushSize(__temp_arena, (size))
#define PushTempStruct(struct) (struct *) PushSize(__temp_arena, sizeof(struct))
#define PushTempArray(struct, length) (struct *) PushSize(__temp_arena, (length) * sizeof(struct))
#define PushTempCopy(...) PushCopy(__temp_arena, __VA_ARGS__)

MemoryArenaBlock *PushTempBlock(u64 size) {
   MemoryArenaBlock *result = (MemoryArenaBlock *) PushSize(__temp_arena, sizeof(MemoryArenaBlock) + size);
   result->size = size;
   result->used = 0;
   result->next = NULL;
   result->memory = (u8 *) (result + 1);
   return result;
}

MemoryArena *PushTempArena(u32 size) {
   MemoryArena *result = PushTempStruct(MemoryArena);
   result->first_block = PushTempBlock(size);
   result->curr_block = result->first_block;
   result->initial_size = size;
   result->valid = true;
   result->alloc_block = PushTempBlock;
   return result;
}

#endif