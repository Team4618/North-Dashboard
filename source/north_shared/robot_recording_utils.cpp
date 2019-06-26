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
   // u32 unit_id; //TODO: generate these
   
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

//-------------------------PARSERS-------------------------
void parseVertexData(LoadedRobotRecording *state, LoadedRecording *recording, buffer *file, bool counting) {
   RobotRecording_VertexData *vdata = ConsumeStruct(file, RobotRecording_VertexData);
   v2 *points = ConsumeArray(file, v2, vdata->point_count);

   if(counting) {
      recording->vdata.vdata_count++;
   } else {
      MemoryArena *arena = state->arena;
      if(recording->vdata.vdata == NULL) {
         recording->vdata.vdata = PushArray(arena, VertexDataRecording, recording->vdata.vdata_count);
         recording->vdata.vdata_count = 0;
      }

      VertexDataRecording *vdata_rec = recording->vdata.vdata + recording->vdata.vdata_count++;
      vdata_rec->point_count = vdata->point_count;
      vdata_rec->points = PushArrayCopy(arena, v2, points, vdata->point_count);
      vdata_rec->colour = vdata->colour;
      vdata_rec->begin_time = vdata->begin_time;
      vdata_rec->end_time = vdata->end_time;

      state->min_time = Min(state->min_time, vdata->begin_time);
      state->max_time = Max(state->max_time, vdata->end_time);
   }
}

void parseRobotPose(LoadedRobotRecording *state, LoadedRecording *recording, buffer *file, bool counting) {
   RobotRecording_RobotPose *pose_data = ConsumeStruct(file, RobotRecording_RobotPose);
   RobotRecording_RobotStateSample *samples = ConsumeArray(file, RobotRecording_RobotStateSample, pose_data->sample_count);

   if(counting) {
      recording->pose.sample_count += pose_data->sample_count;
   } else {
      MemoryArena *arena = state->arena;
      if(recording->pose.samples == NULL) {
         recording->pose.samples = PushArray(arena, RobotRecording_RobotStateSample, recording->pose.sample_count);
         recording->pose.sample_count = 0;
      }

      Copy(samples, pose_data->sample_count * sizeof(RobotRecording_RobotStateSample), 
           recording->pose.samples + recording->pose.sample_count);

      recording->pose.sample_count += pose_data->sample_count;

      for(u32 i = 0; i < pose_data->sample_count; i++) {
         state->min_time = Min(state->min_time, samples[i].time);
         state->max_time = Max(state->max_time, samples[i].time);
      }
   }
}

void parseDiagnostic(LoadedRobotRecording *state, LoadedRecording *recording, buffer *file, bool counting) {
   RobotRecording_Diagnostic *diag_data = ConsumeStruct(file, RobotRecording_Diagnostic);
   string unit_name = ConsumeString(file, diag_data->unit_length);
   RobotRecording_DiagnosticSample *samples = ConsumeArray(file, RobotRecording_DiagnosticSample, diag_data->sample_count);

   if(counting) {
      recording->diag.sample_count += diag_data->sample_count;
   } else {
      MemoryArena *arena = state->arena;
      if(recording->diag.samples == NULL) {
         recording->diag.unit = PushCopy(arena, unit_name);
         recording->diag.samples = PushArray(arena, RobotRecording_DiagnosticSample, recording->diag.sample_count);
         recording->diag.sample_count = 0;
      }

      Copy(samples, diag_data->sample_count * sizeof(RobotRecording_DiagnosticSample), 
           recording->diag.samples + recording->diag.sample_count);

      recording->diag.sample_count += diag_data->sample_count;

      for(u32 i = 0; i < diag_data->sample_count; i++) {
         state->min_time = Min(state->min_time, samples[i].time);
         state->max_time = Max(state->max_time, samples[i].time);
      }
   }
}

void parseMessage(LoadedRobotRecording *state, LoadedRecording *recording, buffer *file, bool counting) {
   RobotRecording_Message *msg = ConsumeStruct(file, RobotRecording_Message);
   string text = ConsumeString(file, msg->text_length);

   if(counting) {
      recording->msg.message_count++;
   } else {
      MemoryArena *arena = state->arena;
      if(recording->msg.messages == NULL) {
         recording->msg.messages = PushArray(arena, MessageRecording, recording->msg.message_count);
         recording->msg.message_count = 0;
      }

      MessageRecording *msg_rec = recording->msg.messages + recording->msg.message_count++;
      msg_rec->text = PushCopy(arena, text);
      msg_rec->begin_time = msg->begin_time;
      msg_rec->end_time = msg->end_time;

      state->min_time = Min(state->min_time, msg->begin_time);
      state->max_time = Max(state->max_time, msg->end_time);
   }
}
//---------------------------------------------------------

typedef void (*entry_parser_callback)(LoadedRobotRecording *state, LoadedRecording *recording, buffer *file, bool count);
extern entry_parser_callback entry_parsers[North_VisType::TypeCount];

entry_parser_callback entry_parsers[North_VisType::TypeCount] = {
   NULL, //Invalid 0
   parseVertexData, //Polyline = 1
   parseVertexData, //Triangles = 2
   parseVertexData, //Points = 3
   parseRobotPose, //RobotPose = 4
   parseDiagnostic, //Diagnostic = 5
   parseMessage, //Message = 6
};

bool hasEntryParser(u8 type) {
   return (type < North_VisType::TypeCount) && (entry_parsers[type] != NULL);
}

void LoadChunk(LoadedRobotRecording *state, buffer *file, bool count) {
   MemoryArena *arena = state->arena;
   RobotRecording_Chunk *chunk = ConsumeStruct(file, RobotRecording_Chunk);

   for(u32 i = 0; i < chunk->entry_count; i++) {
      RobotRecording_Entry *entry = ConsumeStruct(file, RobotRecording_Entry);
      string name = ConsumeString(file, entry->name_length);

      if(hasEntryParser(entry->type)) {
         LoadedRecording *recording = GetOrCreateLoadedRecording(state, name);
         recording->type = (North_VisType::type) entry->type;
         
         for(u32 j = 0; j < entry->recording_count; j++) {
            entry_parsers[entry->type](state, recording, file, count);
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

//---------------------------------------------------
u32 WriteVertexData(buffer *chunk, RecorderEntry *entry) {
   Assert((entry->type == North_VisType::Polyline) ||
          (entry->type == North_VisType::Triangles) ||
          (entry->type == North_VisType::Points));

   //TODO: this writes these in reverse chronological order
   u32 recording_count = 0;
   for(RecorderVertexData *vdata = entry->curr_vdata; vdata; vdata = vdata->next) {
      RobotRecording_VertexData file_vdata = {};
      file_vdata.point_count = vdata->point_count;
      file_vdata.colour = vdata->colour;
      file_vdata.begin_time = vdata->begin_time;
      file_vdata.end_time = vdata->end_time;
      WriteStruct(chunk, &file_vdata);
      WriteArray(chunk, vdata->points, vdata->point_count);
      
      recording_count++;
   }

   entry->curr_vdata = NULL;
   return recording_count;
}

u32 WriteRobotPose(buffer *chunk, RecorderEntry *entry) {
   Assert(entry->type == North_VisType::RobotPose);

   //TODO: this writes these in reverse chronological order
   u32 recording_count = 0;
   for(RecorderPoses *poses = entry->curr_poses; poses; poses = poses->next) {
      RobotRecording_RobotPose file_poses = {};
      file_poses.sample_count = poses->sample_count;
      WriteStruct(chunk, &file_poses);
      WriteArray(chunk, poses->samples, poses->sample_count);
      
      recording_count++;
   }

   entry->curr_poses = NULL;
   return recording_count;
}

u32 WriteDiagnostics(buffer *chunk, RecorderEntry *entry) {
   Assert(entry->type == North_VisType::Diagnostic);

   //TODO: this writes these in reverse chronological order
   u32 recording_count = 0;
   for(RecorderDiagnostic *diags = entry->curr_diag; diags; diags = diags->next) {
      RobotRecording_Diagnostic file_diags = {};
      file_diags.unit_length = diags->unit.length;
      file_diags.sample_count = diags->sample_count;
      WriteStruct(chunk, &file_diags);
      WriteString(chunk, diags->unit);
      WriteArray(chunk, diags->samples, diags->sample_count);
      
      recording_count++;
   }

   entry->curr_diag = NULL;
   return recording_count;
}

u32 WriteMessage(buffer *chunk, RecorderEntry *entry) {
   Assert(entry->type == North_VisType::Message);

   //TODO: this writes these in reverse chronological order
   u32 recording_count = 0;
   for(RecorderMessage *msg = entry->curr_msg; msg; msg = msg->next) {
      RobotRecording_Message file_msg = {};
      file_msg.text_length = msg->text.length;
      file_msg.begin_time = msg->begin_time;
      file_msg.end_time = msg->end_time;
      WriteStruct(chunk, &file_msg);
      WriteString(chunk, msg->text);
      
      recording_count++;
   }

   entry->curr_msg = NULL;
   return recording_count;
}
//---------------------------------------------------

typedef u32 (*entry_writer_callback)(buffer *chunk, RecorderEntry *entry);
extern entry_writer_callback entry_writers[North_VisType::TypeCount];

entry_writer_callback entry_writers[North_VisType::TypeCount] = {
   NULL, //Invalid 0
   WriteVertexData, //Polyline = 1
   WriteVertexData, //Triangles = 2
   WriteVertexData, //Points = 3
   WriteRobotPose, //RobotPose = 4
   WriteDiagnostics, //Diagnostic = 5
   WriteMessage, //Message = 6
};

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

//---------
void RecieveVertexData(buffer *packet, RobotRecorder *state, string name, f32 curr_time, North_VisType::type type) {
   MemoryArena *arena = state->arena;
   Vis_VertexData *vdata = ConsumeStruct(packet, Vis_VertexData);
   v2 *points = ConsumeArray(packet, v2, vdata->point_count);

   RecorderEntry *entry = GetOrCreateEntry(state, name, type, true);
   bool equivalent = true; //TODO: Compare hashes instead of comparing each point
   if(entry->curr_vdata != NULL) {
      equivalent = (vdata->colour == entry->curr_vdata->colour) && 
                   (vdata->point_count == entry->curr_vdata->point_count);

      if(equivalent) {
         for(u32 i = 0; i < vdata->point_count; i++) {
            if(points[i] != entry->curr_vdata->points[i]) {
               equivalent = false;
               break;
            }
         }
      }
   }

   if((entry->curr_vdata == NULL) || !equivalent || entry->ended_recording) {
      if(entry->curr_vdata != NULL)
         entry->curr_vdata->end_time = curr_time;
      
      RecorderVertexData *new_vdata = PushStruct(arena, RecorderVertexData);
      new_vdata->point_count = vdata->point_count;
      new_vdata->points = PushArrayCopy(arena, v2, points, vdata->point_count);
      new_vdata->colour = vdata->colour;
      new_vdata->begin_time = curr_time;
      new_vdata->end_time = curr_time;

      new_vdata->next = entry->curr_vdata;
      entry->curr_vdata = new_vdata;
      entry->ended_recording = false;
   } else if(equivalent) {
      entry->curr_vdata->end_time = curr_time;
   }
}

void RecieveVisRobotPose(buffer *packet, RobotRecorder *state, string name, f32 curr_time, North_VisType::type type) {
   MemoryArena *arena = state->arena;
   Vis_RobotPose *pose = ConsumeStruct(packet, Vis_RobotPose);
   
   RecorderEntry *entry = GetOrCreateEntry(state, name, type, false);
   if((entry->curr_poses == NULL) || (entry->curr_poses->sample_count == ArraySize(entry->curr_poses->samples))) {
      RecorderPoses *new_poses = PushStruct(arena, RecorderPoses);
      new_poses->next = entry->curr_poses;
      entry->curr_poses = new_poses;
   }

   entry->curr_poses->samples[entry->curr_poses->sample_count].time = curr_time;
   entry->curr_poses->samples[entry->curr_poses->sample_count].pos = pose->pos;
   entry->curr_poses->samples[entry->curr_poses->sample_count].angle = pose->angle;
   entry->curr_poses->sample_count++;
}

void RecieveVisDiagnostic(buffer *packet, RobotRecorder *state, string name, f32 curr_time, North_VisType::type type) {
   MemoryArena *arena = state->arena;
   Vis_Diagnostic *diag = ConsumeStruct(packet, Vis_Diagnostic);
   string unit_name = ConsumeString(packet, diag->unit_name_length);

   RecorderEntry *entry = GetOrCreateEntry(state, name, type, false);
   if((entry->curr_diag == NULL) || (entry->curr_diag->sample_count == ArraySize(entry->curr_diag->samples))) {
      RecorderDiagnostic *new_diag = PushStruct(arena, RecorderDiagnostic);
      new_diag->unit = PushCopy(arena, unit_name);

      new_diag->next = entry->curr_diag;
      entry->curr_diag = new_diag;
   }

   entry->curr_diag->samples[entry->curr_diag->sample_count].time = curr_time;
   entry->curr_diag->samples[entry->curr_diag->sample_count].value = diag->value;
   entry->curr_diag->sample_count++;
}

void RecieveVisMessage(buffer *packet, RobotRecorder *state, string name, f32 curr_time, North_VisType::type type) {
   MemoryArena *arena = state->arena;
   Vis_Message *msg = ConsumeStruct(packet, Vis_Message);
   string text = ConsumeString(packet, msg->text_length);

   RecorderEntry *entry = GetOrCreateEntry(state, name, type, true/*time based*/);
   if((entry->curr_msg == NULL) || (entry->curr_msg->text != text) || entry->ended_recording) {
      if(entry->curr_msg != NULL)
         entry->curr_msg->end_time = curr_time;
      
      RecorderMessage *new_recording = PushStruct(arena, RecorderMessage);
      new_recording->text = PushCopy(arena, text);
      new_recording->begin_time = curr_time;
      new_recording->end_time = curr_time;

      new_recording->next = entry->curr_msg;
      entry->curr_msg = new_recording;
      entry->ended_recording = false;
   } else if(entry->curr_msg->text == text) {
      entry->curr_msg->end_time = curr_time;
   }
}
//-----------

typedef void (*entry_reciever_callback)(buffer *packet, RobotRecorder *state, string name, f32 curr_time, North_VisType::type type);
entry_reciever_callback entry_recievers[North_VisType::TypeCount] = {
   NULL, //Invalid 0
   RecieveVertexData, //Polyline = 1
   RecieveVertexData, //Triangles = 2
   RecieveVertexData, //Points = 3
   RecieveVisRobotPose, //RobotPose = 4
   RecieveVisDiagnostic, //Diagnostic = 5
   RecieveVisMessage, //Message = 6
};

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

//UI----------------------------------
#ifdef INCLUDE_DRAWRECORDINGS

//-----------------------------------
void DrawPointsRecording(element *page, ui_field_topdown *field, LoadedRobotRecording *state, LoadedRecording *rec) {
   if(page) {
      Label(page, rec->name, 20, BLACK);
   }

   if(!rec->hidden) {
      for(u32 i = 0; i < rec->vdata.vdata_count; i++) {
         VertexDataRecording *vdata = rec->vdata.vdata + i;
         if((vdata->begin_time <= state->curr_time) && (state->curr_time <= vdata->end_time)) {
            //Replace this with one call that takes the points & a transform matrix
            for(u32 j = 0; j < vdata->point_count; j++) {
               v2 p = GetPoint(field, vdata->points[j]);
               Rectangle(field->e, RectCenterSize(p, V2(5, 5)), vdata->colour);
            }
            //------------------------------
         }
      }
   }
}

void DrawPolylineRecording(element *page, ui_field_topdown *field, LoadedRobotRecording *state, LoadedRecording *rec) {
   if(page) {
      Label(page, rec->name, 20, BLACK);
   }

   for(u32 i = 0; i < rec->vdata.vdata_count; i++) {
      VertexDataRecording *vdata = rec->vdata.vdata + i;
      if((vdata->begin_time <= state->curr_time) && (state->curr_time <= vdata->end_time)) {
         
         v2 *transformed_points = PushTempArray(v2, vdata->point_count);
         for(u32 j = 0; j < vdata->point_count; j++) {
            transformed_points[j] = GetPoint(field, transformed_points[j]);
         }
         
         _Line(field->e, vdata->colour, 3, transformed_points, vdata->point_count);
      }
   }
}

void DrawRobotPoseRecording(element *page, ui_field_topdown *field, LoadedRobotRecording *state, LoadedRecording *rec) {
   if(page) {
      Label(page, rec->name, 20, BLACK);
   }
   
   for(u32 i = 0; i < rec->pose.sample_count; i++) {
      RobotRecording_RobotStateSample sample = rec->pose.samples[i];
      if(sample.time >= state->curr_time) {
         DrawRobot(field, V2(0.5, 0.5), sample.pos, sample.angle, BLACK);
         break;
      }
   }
}

void DrawMessageRecording(element *page, ui_field_topdown *field, LoadedRobotRecording *state, LoadedRecording *rec) {
   if(page) {
      Label(page, rec->name, 20, BLACK);
      
      for(u32 i = 0; i < rec->msg.message_count; i++) {
         MessageRecording *msg = rec->msg.messages + i;
         if((msg->begin_time <= state->curr_time) && (state->curr_time <= msg->end_time)) {
            Label(page, msg->text, 20, BLACK, V2(20, 0));
         }
      }
   }
}
//-----------------------------------

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
      if(rec->type == North_VisType::Points) {
         DrawPointsRecording(child_page, field, state, rec);
      } else if(rec->type == North_VisType::RobotPose) {
         DrawRobotPoseRecording(child_page, field, state, rec);
      } else if(rec->type == North_VisType::Polyline) {
         DrawPolylineRecording(child_page, field, state, rec);
      } else if(rec->type == North_VisType::Message) {
         DrawMessageRecording(child_page, field, state, rec);
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

#endif
//------------------------------------