//PARSING-NCRR-FILES--------------------------------
//NOTE: used for polylines, triangles & points
struct VertexDataRecording {
   v2 *points;
   u32 point_count;
   
   v4 colour;

   f32 begin_time;
   f32 end_time;
};

struct DiagnosticRecording {
   string unit;
   
   u32 sample_count;
   RobotRecording_DiagnosticSample *samples;
};

struct MessageRecording {
   string text;
   f32 begin_time;
   f32 end_time;
};

struct LoadedRecording {
   LoadedRecording *next;
   North_VisType::type type;
   string name;

   bool hidden;

   union {
      struct {
         u32 vdata_count;
         VertexDataRecording *vdata;
      } vdata;

      struct {
         u32 sample_count;
         RobotRecording_RobotStateSample *samples;
      } pose;

      DiagnosticRecording diag;

      struct {
         u32 message_count;
         MessageRecording *messages;
      } msg;
   };
};

struct RecordingNamespace {
   RecordingNamespace *next;
   string name;

   bool collapsed;

   RecordingNamespace *first_child;
   LoadedRecording *first_recording;
};

struct LoadedRobotRecording {
   MemoryArena *arena; //NOTE: owned by LoadedRobotRecording
   bool loaded;

   string robot_name;
   u64 timestamp;

   f32 min_time;
   f32 max_time;
   f32 curr_time;

   RecordingNamespace root_namespace;
};

LoadedRecording *GetOrCreateLoadedRecording(LoadedRobotRecording *state, string name) {
   MemoryArena *arena = state->arena;
   SplitString path = split(name, '/');
   Assert(path.part_count > 0);

   RecordingNamespace *curr_namespace = &state->root_namespace;
   for(u32 i = 0; i < path.part_count - 1; i++) {
      string ns_name = path.parts[i];
      bool found = false;

      for(RecordingNamespace *ns = curr_namespace->first_child; ns; ns = ns->next) {
         if(ns->name == ns_name) {
            curr_namespace = ns;
            found = true;
            break;
         }
      }

      if(!found) {
         RecordingNamespace *new_ns = PushStruct(arena, RecordingNamespace);
         new_ns->name = PushCopy(arena, ns_name);
         new_ns->next = curr_namespace->first_child;
         curr_namespace->first_child = new_ns;
         
         curr_namespace = new_ns;
      }
   }

   LoadedRecording *result = NULL;
   string rec_name = path.parts[path.part_count - 1];
   for(LoadedRecording *rec = curr_namespace->first_recording; rec; rec = rec->next) {
      if(rec->name == rec_name) {
         result = rec;
         break;
      }
   }

   if(result == NULL) {
      result = PushStruct(arena, LoadedRecording);
      result->name = PushCopy(arena, rec_name);
      result->next = curr_namespace->first_recording;
      curr_namespace->first_recording = result;
   }

   return result;
}

typedef void (*entry_parser_callback)(LoadedRobotRecording *state, LoadedRecording *recording, buffer *file, bool count);
extern entry_parser_callback entry_parsers[North_VisType::TypeCount];

entry_parser_callback getEntryParser(u8 type) {
   return (type < ArraySize(entry_parsers)) ? entry_parsers[type] : NULL;
}

void LoadChunk(LoadedRobotRecording *state, buffer *file, bool count) {
   MemoryArena *arena = state->arena;
   RobotRecording_Chunk *chunk = ConsumeStruct(file, RobotRecording_Chunk);

   for(u32 i = 0; i < chunk->entry_count; i++) {
      RobotRecording_Entry *entry = ConsumeStruct(file, RobotRecording_Entry);
      string name = ConsumeString(file, entry->name_length);

      entry_parser_callback parse_func = getEntryParser(entry->type);
      if(parse_func) {
         LoadedRecording *recording = GetOrCreateLoadedRecording(state, name);
         recording->type = (North_VisType::type) entry->type;
         
         for(u32 j = 0; j < entry->recording_count; j++) {
            parse_func(state, recording, file, count);
         }
      } else {
         //TODO: report unknown entry type?
         ConsumeSize(file, entry->size);
      }
   }
}

void LoadRecording(LoadedRobotRecording *state, string file_name) {
   MemoryArena *arena = state->arena;
   
   Reset(arena);
   ZeroStruct(&state->root_namespace);
   state->robot_name = {};

   buffer file = ReadEntireFile(Concat(file_name, Literal(".ncrr")));
   state->loaded = (file.data != NULL);
   if(file.data != NULL) {
      FileHeader *file_numbers = ConsumeStruct(&file, FileHeader);

      if((file_numbers->magic_number == ROBOT_RECORDING_MAGIC_NUMBER) && 
         (file_numbers->version_number == ROBOT_RECORDING_CURR_VERSION))
      {
         RobotRecording_FileHeader *header = ConsumeStruct(&file, RobotRecording_FileHeader);
         
         state->timestamp = header->timestamp;
         state->robot_name = ConsumeAndCopyString(&file, header->robot_name_length, arena);
         state->min_time = F32_MAX;
         state->max_time = -F32_MAX;
         
         //This will count the data
         buffer count_file = file;
         for(u32 i = 0; i < header->chunk_count; i++) {
            LoadChunk(state, &count_file, true);
         }

         //This will allocate and load the data
         for(u32 i = 0; i < header->chunk_count; i++) {
            LoadChunk(state, &file, false);
         }

         state->curr_time = state->min_time;
      }
   }
}

//RECORDING-------------------------------------
struct RecorderVertexData {
   RecorderVertexData *next;

   v2 *points;
   u32 point_count;
   
   v4 colour;

   f32 begin_time;
   f32 end_time;
};

struct RecorderPoses {
   RecorderPoses *next;

   u32 sample_count;
   RobotRecording_RobotStateSample samples[1024];
};

struct RecorderDiagnostic {
   RecorderDiagnostic *next;
   
   string unit;

   u32 sample_count;
   RobotRecording_DiagnosticSample samples[1024];
};

struct RecorderMessage {
   RecorderMessage *next;

   string text;
   f32 begin_time;
   f32 end_time;
};

struct RecorderEntry {
   RecorderEntry *next;
   North_VisType::type type;
   string name;
   bool time_based; 

   bool accessed;
   bool ended_recording;
   
   union {
      RecorderVertexData *curr_vdata;
      RecorderDiagnostic *curr_diag;
      RecorderPoses *curr_poses;
      RecorderMessage *curr_msg;
   };
};

struct RobotRecorder {
   //TODO: replace entry_arena with a freelist of entries (what do we do about the name strings?)
   // MemoryArena *entry_arena;
   // MemoryArena *recording_arena; 
   
   MemoryArena *arena;
   bool recording;
   u64 latest_frame;

   RecorderEntry *first_entry;

   u64 curr_file;
   RobotRecording_FileHeader curr_header;
};

//TODO: accelerate this function with a hash table (instead of a list search)
RecorderEntry *GetOrCreateEntry(RobotRecorder *state, string name, North_VisType::type type, bool time_based) {
   RecorderEntry *result = NULL;
   for(RecorderEntry *entry = state->first_entry; entry; entry = entry->next) {
      if(entry->name == name)
         result = entry;
   }

   if(result == NULL) {
      MemoryArena *arena = state->arena;
      result = PushStruct(arena, RecorderEntry);
      result->name = PushCopy(arena, name);
      result->type = type;
      result->time_based = time_based;

      result->next = state->first_entry;
      state->first_entry = result;
   }

   result->accessed = true;
   return result;
}

typedef u32 (*entry_writer_callback)(buffer *chunk, RecorderEntry *entry);
extern entry_writer_callback entry_writers[North_VisType::TypeCount];

void WriteChunk(RobotRecorder *state) {
   buffer chunk = PushTempBuffer(Megabyte(5));

   RobotRecording_Chunk *chunk_header = PushStruct(&chunk, RobotRecording_Chunk);
   for(RecorderEntry *entry = state->first_entry; entry; entry = entry->next) {
      RobotRecording_Entry *entry_header = PushStruct(&chunk, RobotRecording_Entry);
      entry_header->type = (u8)entry->type;
      entry_header->name_length = entry->name.length;
      WriteString(&chunk, entry->name);
      u64 begining_offset = chunk.offset;

      //Writes to the chunk and flushes the entry's recordings
      entry_header->recording_count = entry_writers[entry->type](&chunk, entry);
      
      entry_header->size = chunk.offset - begining_offset;
      chunk_header->entry_count++;  
   }

   //TODO: reset recording_arena
   //TODO: get rid of any entries that had no recordings?

   //Increment chunk_count in file header
   state->curr_header.chunk_count++;
   WriteFileRange(state->curr_file, Buffer(sizeof(RobotRecording_FileHeader), (u8 *) &state->curr_header, sizeof(RobotRecording_FileHeader)), sizeof(FileHeader));
   
   //Append chunk to file
   WriteFileAppend(state->curr_file, chunk);
}

typedef void (*entry_reciever_callback)(buffer *packet, RobotRecorder *state, string name, f32 curr_time, North_VisType::type type);
extern entry_reciever_callback entry_recievers[North_VisType::TypeCount];

bool hasEntryReciever(u8 type) {
   return (type < North_VisType::TypeCount) && (entry_recievers[type] != NULL);
}

void ParseVisEntry(RobotRecorder *state, f32 curr_time, buffer *packet) {
   Vis_Entry *entry_header = ConsumeStruct(packet, Vis_Entry);
   string name = ConsumeString(packet, entry_header->name_length);

   if(hasEntryReciever(entry_header->type)) {
      entry_recievers[entry_header->type](packet, state, name, curr_time, (North_VisType::type)entry_header->type);
   } else {
      //TODO: report unknown entry type?
      ConsumeSize(packet, entry_header->size);
   }
}

void BeginUpdates(RobotRecorder *state) {
   for(RecorderEntry *entry = state->first_entry; entry; entry = entry->next) {
      entry->accessed = false;
   }
}

void EndUpdates(RobotRecorder *state) {
   for(RecorderEntry *entry = state->first_entry; entry; entry = entry->next) {
      if(entry->time_based && !entry->accessed) {
         entry->ended_recording = true;
      }
   }
}

void RecieveStatePacket(RobotRecorder *state, buffer packet) {
   if(!state->recording)
      return;

   MemoryArena *arena = state->arena;
   VisHeader *header = ConsumeStruct(&packet, VisHeader);

   u64 curr_frame = header->frame_number;
   f32 curr_time = header->time;

   if(state->latest_frame != curr_frame) {
      EndUpdates(state);
      BeginUpdates(state);
      state->latest_frame = curr_frame;
   }

   for(u32 i = 0; i < header->entry_count; i++) {
      ParseVisEntry(state, curr_time, &packet);
   }
}

void BeginRecording(RobotRecorder *state) {
   Assert(!state->recording);
   state->recording = true;

   Reset(state->arena);
   state->latest_frame = 0;
   state->first_entry = NULL;

   //TODO: get real data for this
      u64 timestamp = 10;
      string robot_name = Literal("test_robot");
      f32 robot_width = 0.25;
      f32 robot_length = 0.25;
      string recording_name = Literal("test_recording");
   
   state->curr_header.timestamp = timestamp;
   state->curr_header.robot_name_length = robot_name.length;
   state->curr_header.robot_width = robot_width;
   state->curr_header.robot_length = robot_length;
   state->curr_header.chunk_count = 0;

   buffer file_start = PushTempBuffer(Kilobyte(10));
   FileHeader numbers = header(ROBOT_RECORDING_MAGIC_NUMBER, ROBOT_RECORDING_CURR_VERSION);
   WriteStruct(&file_start, &numbers);
   WriteStruct(&file_start, &state->curr_header);
   WriteString(&file_start, robot_name);

   state->curr_file = CreateFile(Concat(recording_name, Literal(".ncrr")));
   WriteFileAppend(state->curr_file, file_start);
}

void EndRecording(RobotRecorder *state, string recording_name) {
   Assert(state->recording);
   state->recording = false;

   WriteChunk(state);
   CloseFile(state->curr_file);
}

#ifdef INCLUDE_DRAWRECORDINGS
//UI----------------------------------
typedef void (*recording_draw_callback)(element *page, ui_field_topdown *field, LoadedRobotRecording *state, LoadedRecording *rec);
extern recording_draw_callback recording_drawers[North_VisType::TypeCount];

recording_draw_callback getRecordingDrawCallback(u8 type) {
   return (type < ArraySize(recording_drawers)) ? recording_drawers[type] : NULL;
}

void DrawRecordingNamespace(element *page, ui_field_topdown *field, LoadedRobotRecording *state, RecordingNamespace *ns) {
   element *child_page = NULL;
   
   if(page) {
      UI_SCOPE(page, ns);
      button_style hide_button = ButtonStyle(
         dark_grey, light_grey, BLACK,
         light_grey, V4(120/255.0, 120/255.0, 120/255.0, 1), WHITE, 
         off_white, light_grey,
         20, V2(0, 0), V2(0, 0));

      element *top_row = RowPanel(page, Size(Size(page).x, 20));
      Label(top_row, (ns->name.length == 0) ? Literal("/") : ns->name, 20, BLACK);
      if(Button(top_row, ns->collapsed ? "  +  " : "  -  ", hide_button).clicked) {
         ns->collapsed = !ns->collapsed;
      }

      child_page = ns->collapsed ? NULL : page;
   }
   
   for(LoadedRecording *rec = ns->first_recording; rec; rec = rec->next) {
      recording_draw_callback draw_func = getRecordingDrawCallback(rec->type);
      if(draw_func) {
         draw_func(child_page, field, state, rec);
      }
   }
   
   for(RecordingNamespace *child = ns->first_child; child; child = child->next) {
      DrawRecordingNamespace(child_page, field, state, child);
   }
}

void DrawRecordings(element *full_page, FileListLink *ncrr_files, LoadedRobotRecording *recording, 
                    NorthSettings *settings, RobotProfiles *profiles)
{
   StackLayout(full_page);
   element *page = VerticalList(full_page);
   
   element *top_bar = RowPanel(page, Size(Size(page).x - 10, page_tab_height).Padding(5, 5));
   Background(top_bar, dark_grey);
   static bool selector_open = false;

   if(Button(top_bar, "Open Recording", menu_button.IsSelected(selector_open)).clicked) {
      selector_open = !selector_open;
   }

   if(selector_open) {
      for(FileListLink *file = ncrr_files; file; file = file->next) {
         UI_SCOPE(page->context, file);
         
         if(Button(page, file->name, menu_button).clicked) {
            LoadRecording(recording, file->name);
            selector_open = false;
         }
      }
   } else {
      if(recording->loaded) {
         static bool is_playing = false;
         if(Button(top_bar, is_playing ? "Stop" : "Play", menu_button).clicked) {
            is_playing = !is_playing;
         }

         if(is_playing) {
            recording->curr_time += page->context->dt;
            if(recording->curr_time > recording->max_time) {
               is_playing = false;
               recording->curr_time = recording->max_time;
            }
         }

         ui_field_topdown field = {};

         if(settings->field.loaded) {
            field = FieldTopdown(page, settings->field.image, settings->field.size, 
                                 Clamp(0, Size(page->bounds).x, 700));

            DrawRecordingNamespace(page, &field, recording, &recording->root_namespace);
            
            //TODO: automatically center this somehow, maybe make a CenterColumnLayout?
            HorizontalSlider(page, &recording->curr_time, recording->min_time, recording->max_time,
                             V2(Size(page->bounds).x - 60, 40), V2(20, 20));
         
         } else {
            Label(page, "No field loaded", 20, BLACK);
         }
      } else {   
         selector_open = true;
      }
   }
}
//------------------------------------
#endif

#include "robot_recording_utils_callbacks.cpp"