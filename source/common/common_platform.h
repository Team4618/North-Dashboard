#if !defined(COMMON_PLATFORM) && !defined(NO_COMMON_PLATFORM)
#define COMMON_PLATFORM

#include "common_types.h"
#include "common_memory.h"
#include "common_strings.h"
#include "common_buffer.h"

string exe_directory = {};

struct NamedMemoryArena {
   //NOTE: NamedMemoryArena, the name string & the arena are all allocated together
   NamedMemoryArena *next;

   string name;
   MemoryArena arena;
};

NamedMemoryArena *mdbg_first_arena = NULL;

#if defined(_WIN32)
   //TODO: make stuff thread safe before we re-introduce this stuff

   //NOTE: on windows you need to include "windows.h" before common
   // u32 AtomicIncrement(volatile u32 *x) {
   //    Assert(( (u64)x & 0x3 ) == 0);
   //    return InterlockedIncrement(x);
   // }
   // #define READ_BARRIER MemoryBarrier()
   // #define WRITE_BARRIER MemoryBarrier()

   MemoryArenaBlock *PlatformAllocArenaBlock(u64 size) {
      MemoryArenaBlock *result = (MemoryArenaBlock *) VirtualAlloc(0, sizeof(MemoryArenaBlock) + size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
      result->size = size;
      result->used = 0;
      result->next = NULL;
      result->memory = (u8 *) (result + 1);
      return result;
   }

   MemoryArena *PlatformAllocArena(u64 initial_size, string name) {
      //NOTE: big joint allocation here
      //TODO: make joint allocations easier??
      u8 *memory = (u8 *) VirtualAlloc(0, 
         sizeof(NamedMemoryArena) + name.length + sizeof(MemoryArenaBlock) + initial_size, 
         MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
      
      NamedMemoryArena *named_arena = (NamedMemoryArena *) memory;
      u8 *string_text = (u8 *) (memory + sizeof(NamedMemoryArena));
      MemoryArenaBlock *first_block = (MemoryArenaBlock *) (memory + sizeof(NamedMemoryArena) + name.length);
      u8 *block_memory = (u8 *) (memory + sizeof(NamedMemoryArena) + name.length + sizeof(MemoryArenaBlock));

      //NOTE: initializing everything
      first_block->size = initial_size;
      first_block->used = 0;
      first_block->next = NULL;
      first_block->memory = block_memory;
      
      named_arena->name = String((char *) string_text, name.length);
      Copy(name.text, name.length, named_arena->name.text); 

      ZeroStruct(&named_arena->arena);
      named_arena->arena.first_block = first_block;
      named_arena->arena.curr_block = first_block;
      named_arena->arena.initial_size = initial_size;
      named_arena->arena.valid = true;
      named_arena->arena.alloc_block = PlatformAllocArenaBlock;

      named_arena->next = mdbg_first_arena;
      mdbg_first_arena = named_arena;

      return &named_arena->arena;
   }

   MemoryArena *PlatformAllocArena(u64 initial_size, char *name) {
      return PlatformAllocArena(initial_size, Literal(name));
   }

   //TODO: ReadFileRange()

   buffer ReadEntireFile(const char* path, bool in_exe_directory = false, MemoryArena *arena = __temp_arena) {
      char full_path[MAX_PATH + 1];
      sprintf(full_path, "%.*s%s", exe_directory.length, exe_directory.text, path);

      HANDLE file_handle = CreateFileA(in_exe_directory ? full_path : path, GENERIC_READ, FILE_SHARE_READ,
                                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                                       
      buffer result = {};
      if(file_handle != INVALID_HANDLE_VALUE) {
         DWORD number_of_bytes_read;
         result.size = GetFileSize(file_handle, NULL);
         result.data = (u8 *) PushSize(arena, result.size);
         ReadFile(file_handle, result.data, result.size, &number_of_bytes_read, NULL);
         CloseHandle(file_handle);
      } else {
         OutputDebugStringA("File read error\n");
         DWORD error_code = GetLastError();
      }

      return result;
   }

   buffer ReadEntireFile(string path, bool in_exe_directory = false) {
      return ReadEntireFile(ToCString(path), in_exe_directory);
   }

   void WriteEntireFile(const char* path, buffer file) {
      HANDLE file_handle = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL, NULL);
                                       
      if(file_handle != INVALID_HANDLE_VALUE) {
         DWORD number_of_bytes_written;
         WriteFile(file_handle, file.data, file.offset, &number_of_bytes_written, NULL);
         CloseHandle(file_handle);
      }
   }

   void WriteEntireFile(string path, buffer file) {
      return WriteEntireFile(ToCString(path), file);
   }

   //TODO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

   //NOTE: this creates a file only if the file didnt already exist
   //      won't totally overwrite existing files like WriteEntireFile
   // void WriteFileRange(const char* path, buffer file, u64 offset) {
   //    HANDLE file_handle = CreateFileA(path, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
   //                                     FILE_ATTRIBUTE_NORMAL, NULL);
                                       
   //    if(file_handle != INVALID_HANDLE_VALUE) {
   //       //TODO: write at offset
   //       // DWORD number_of_bytes_written;
   //       // WriteFile(file_handle, file.data, file.offset, &number_of_bytes_written, NULL);
   //       CloseHandle(file_handle);
   //    }
   // }

   void WriteFileAppend(const char* path, buffer file) {
      HANDLE file_handle = CreateFileA(path, GENERIC_WRITE | FILE_APPEND_DATA, 
                                       0, NULL, OPEN_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL, NULL);
                                       
      if(file_handle != INVALID_HANDLE_VALUE) {
         SetFilePointer(file_handle, 0, NULL, FILE_END);
         DWORD number_of_bytes_written;
         WriteFile(file_handle, file.data, file.offset, &number_of_bytes_written, NULL);
         CloseHandle(file_handle);
      }
   }

   void WriteFileAppend(string path, buffer file) {
      return WriteFileAppend(ToCString(path), file);
   }

   //NEW FILE STUFF---------------------------------
   u64 CreateFile(const char *path) {
      HANDLE file_handle = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL, NULL);
      return (u64) file_handle;
   }

   u64 CreateFile(string path) {
      return CreateFile(ToCString(path));
   }

   void CloseFile(u64 handle) {
      CloseHandle((HANDLE) handle);
   }

   void WriteFileRange(u64 handle, buffer b, u64 file_offset) {
      HANDLE file_handle = (HANDLE) handle;
      
      if(file_handle != INVALID_HANDLE_VALUE) {
         //TODO: this can only do 32 bit offsets right now
         SetFilePointer(file_handle, (u32)file_offset, NULL, FILE_BEGIN);

         DWORD number_of_bytes_written;
         WriteFile(file_handle, b.data, b.offset, &number_of_bytes_written, NULL);
      }
   }

   void WriteFileAppend(u64 handle, buffer b) {
      HANDLE file_handle = (HANDLE) handle;
      
      if(file_handle != INVALID_HANDLE_VALUE) {
         SetFilePointer(file_handle, 0, NULL, FILE_END);
         DWORD number_of_bytes_written;
         WriteFile(file_handle, b.data, b.offset, &number_of_bytes_written, NULL);
      }
   }
   //-----------------------------------------------

   void CreateFolder(const char* path) {
      CreateDirectoryA(path, NULL);
   }

   void CreateFolder(string path) {
      CreateFolder(ToCString(path));
   }

   struct FileListLink {
      FileListLink *next;
      string name;
      string full_name;
   };

   FileListLink *ListFilesWithExtension(char *wildcard_extension, MemoryArena *arena = __temp_arena) {
      WIN32_FIND_DATAA file = {};
      HANDLE handle = FindFirstFileA(wildcard_extension, &file);
      FileListLink *result = NULL;
      if(handle != INVALID_HANDLE_VALUE) {
         do {
            FileListLink *new_link = PushStruct(arena, FileListLink);
            string full_name = Literal(file.cFileName);
            
            if((full_name == Literal(".")) ||
               (full_name == Literal("..")))
            {
               continue;
            }
            
            u32 name_length = 0;
            for(u32 i = 0; i < full_name.length; i++) {
               if(full_name.text[i] == '.') {
                  name_length = i;
                  break;
               }
            }
            
            new_link->full_name = PushCopy(arena, full_name);
            new_link->name = String(new_link->full_name.text, name_length);
            new_link->next = result;
            result = new_link;
         } while(FindNextFileA(handle, &file));
         FindClose(handle);
      }
      return result;
   }

   u64 GetFileTimestamp(const char* path, bool in_exe_directory = false) {
      char full_path[MAX_PATH + 1];
      sprintf(full_path, "%.*s%s", exe_directory.length, exe_directory.text, path);
      
      FILETIME last_write_time = {};
      HANDLE file_handle = CreateFileA(in_exe_directory ? full_path : path, GENERIC_READ, FILE_SHARE_READ,
                                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      
      if(file_handle != INVALID_HANDLE_VALUE) {
         GetFileTime(file_handle, NULL, NULL, &last_write_time);
         CloseHandle(file_handle);
      }
      
      return (u64)last_write_time.dwLowDateTime | 
            ((u64)last_write_time.dwHighDateTime << 32);
   }

   u64 GetFileTimestamp(string path, bool in_exe_directory = false) {
      return GetFileTimestamp(ToCString(path), in_exe_directory);
   }

   struct FileWatcherLink {
      FileWatcherLink *next_in_list;
      FileWatcherLink *next_in_hash;

      bool found;

      u64 timestamp;
      string name;
   };
   
   struct FileWatcher {
      //NOTE: this needs its own memory because it has to persist
      MemoryArena *arena; //NOTE: FileWatcher ownes this
      string wildcard_extension;

      FileWatcherLink *first_in_list;
      FileWatcherLink *hash[64];
   };

   void InitFileWatcher(FileWatcher *watcher, MemoryArena *arena, string wildcard_extension) {
      watcher->arena = arena;
      watcher->wildcard_extension = PushCopy(watcher->arena, wildcard_extension);
   }
   
   void InitFileWatcher(FileWatcher *watcher, MemoryArena *arena, char *wildcard_extension) {
      InitFileWatcher(watcher, arena, Literal(wildcard_extension));
   }

   //NOTE: updates file watcher, returns true if file timestamps have changed
   bool CheckFiles(FileWatcher *watcher) {
      MemoryArena *arena = watcher->arena;

      for(FileWatcherLink *curr = watcher->first_in_list;
            curr; curr = curr->next_in_list)
      {
         curr->found = false;
      }

      bool changed = false;
      for(FileListLink *file = ListFilesWithExtension(ToCString(watcher->wildcard_extension)); 
            file; file = file->next)
      {
         FileWatcherLink *link = NULL;
         u32 hash = Hash(file->full_name) % ArraySize(watcher->hash);
         for(FileWatcherLink *curr = watcher->hash[hash];
               curr; curr = curr->next_in_hash)
         {
            if(curr->name == file->full_name) {
               link = curr;
            }
         }

         if(link == NULL) {
            changed = true;
            FileWatcherLink *new_link = PushStruct(arena, FileWatcherLink);
            new_link->name = PushCopy(arena, file->full_name);
            
            new_link->next_in_list = watcher->first_in_list;
            watcher->first_in_list = new_link;
            new_link->next_in_hash = watcher->hash[hash];
            watcher->hash[hash] = new_link;

            link = new_link;
         }

         link->found = true;
         u64 timestamp = GetFileTimestamp(file->name);
         if(link->timestamp != timestamp) {
            link->timestamp = timestamp;
            changed = true;
         }
      }
      
      for(FileWatcherLink *curr = watcher->first_in_list;
            curr; curr = curr->next_in_list)
      {
         if(curr->found == false) {
            changed = true;
         }
      }
      
      return changed;
   }
   
   //NOTE: this is pretty jank-tastic but itll get cleaned up in future
   char exepath[MAX_PATH + 1];
   void Win32CommonInit(MemoryArena *temp_arena) {
      __temp_arena = temp_arena;

      if(0 == GetModuleFileNameA(0, exepath, MAX_PATH + 1))
         Assert(false);
         
      exe_directory = Literal(exepath);
      for(u32 i = exe_directory.length - 1; i >= 0; i--) {
         if((exe_directory.text[i] == '\\') || (exe_directory.text[i] == '/'))
            break;

         exe_directory.length--;
      }

      //TODO: setup a console for logging when we're not running in visual studios
   }

   struct Timer {
      LARGE_INTEGER frequency;
      LARGE_INTEGER last_time;
   };

   Timer InitTimer() {
      Timer result = {};
      QueryPerformanceFrequency(&result.frequency);
      QueryPerformanceCounter(&result.last_time);
      return result;
   }

   f32 GetDT(Timer *timer) {
      LARGE_INTEGER new_time;
      QueryPerformanceCounter(&new_time);
      f32 dt = (f32)(new_time.QuadPart - timer->last_time.QuadPart) / (f32)timer->frequency.QuadPart;
      timer->last_time = new_time;
      
      return dt;
   }
#else
   #error "we dont support that platform yet"
#endif

// struct ticket_mutex {
//    volatile u32 ticket;
//    volatile u32 serving;
// };

// void BeginMutex(ticket_mutex *mutex) {
//    u32 ticket = AtomicIncrement(&mutex->ticket) - 1;
//    while(mutex->serving != ticket);
// }

// void EndMutex(ticket_mutex *mutex) {
//    AtomicIncrement(&mutex->serving);
// }

//TODO: thread_pool

#endif