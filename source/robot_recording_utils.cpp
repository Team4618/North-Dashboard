//PARSING-NCRR-FILES--------------------------------
struct DiagnosticsRecording {
   string name;
   North_Unit::type unit;
   
   u32 sample_count;
   RobotRecording_DiagnosticSample *samples;
};

struct MessageRecording {
   North_MessageType::type type;
   string text;
   f32 begin_time;
   f32 end_time;
};

struct MarkerRecording {
   v2 pos;
   string text;
   f32 begin_time;
   f32 end_time;
};

struct PathRecording {
   u32 control_point_count;
   North_HermiteControlPoint *control_points;

   string text;
   f32 begin_time;
   f32 end_time;
};

struct LoadedRecordingGroup {
   string name;
   bool collapsed;
   MultiLineGraphData graph;

   u32 diagnostic_count;
   DiagnosticsRecording *diagnostics;

   u32 message_count;
   MessageRecording *messages;

   u32 marker_count;
   MarkerRecording *markers;

   u32 path_count;
   PathRecording *paths;
};

struct LoadedRobotRecording {
   MemoryArena arena;
   bool loaded;

   string robot_name;
   u64 timestamp;

   f32 min_time;
   f32 max_time;
   f32 curr_time;

   u32 robot_sample_count;
   RobotRecording_RobotStateSample *robot_samples;

   LoadedRecordingGroup default_group;
   u32 group_count;
   LoadedRecordingGroup *groups;
};

void LoadRecordingGroup(MemoryArena *arena, buffer *file, LoadedRecordingGroup *group) {
   RobotRecording_Group *header = ConsumeStruct(file, RobotRecording_Group);
   group->name = PushCopy(arena, ConsumeString(file, header->name_length));
   group->collapsed = true;

   //TODO: make this better
   group->graph = NewMultiLineGraph(PlatformAllocArena( Megabyte(4) ));

   group->diagnostic_count = header->diagnostic_count;
   group->diagnostics = PushArray(arena, DiagnosticsRecording, header->diagnostic_count);
   group->message_count = header->message_count;
   group->messages = PushArray(arena, MessageRecording, header->message_count);
   group->marker_count = header->marker_count;
   group->markers = PushArray(arena, MarkerRecording, header->marker_count);
   group->path_count = header->path_count;
   group->paths = PushArray(arena, PathRecording, header->path_count);

   for(u32 i = 0; i < header->diagnostic_count; i++) {
      DiagnosticsRecording *diag = group->diagnostics + i;
      RobotRecording_Diagnostic *diag_header = ConsumeStruct(file, RobotRecording_Diagnostic);
      diag->name = PushCopy(arena, ConsumeString(file, diag_header->name_length));
      diag->unit = (North_Unit::type) diag_header->unit;
      diag->sample_count = diag_header->sample_count;
      diag->samples = ConsumeAndCopyArray(arena, file, RobotRecording_DiagnosticSample, diag->sample_count);

      for(u32 i = 0; i < diag->sample_count; i++) {
         RobotRecording_DiagnosticSample sample = diag->samples[i];
         AddEntry(&group->graph, diag->name, sample.value, sample.time, diag->unit);
      }
   }

   for(u32 i = 0; i < header->message_count; i++) {
      MessageRecording *msg = group->messages + i;
      RobotRecording_Message *msg_header = ConsumeStruct(file, RobotRecording_Message);
      
      msg->begin_time = msg_header->begin_time;
      msg->end_time = msg_header->end_time;
      msg->type = (North_MessageType::type) msg_header->type;
      msg->text = PushCopy(arena, ConsumeString(file, msg_header->length));
   }

   for(u32 i = 0; i < header->marker_count; i++) {
      MarkerRecording *msg = group->markers + i;
      RobotRecording_Marker *msg_header = ConsumeStruct(file, RobotRecording_Marker);
      
      msg->begin_time = msg_header->begin_time;
      msg->end_time = msg_header->end_time;
      msg->pos = msg_header->pos;
      msg->text = PushCopy(arena, ConsumeString(file, msg_header->length));
   }

   for(u32 i = 0; i < header->path_count; i++) {
      PathRecording *msg = group->paths + i;
      RobotRecording_Path *msg_header = ConsumeStruct(file, RobotRecording_Path);
      
      msg->begin_time = msg_header->begin_time;
      msg->end_time = msg_header->end_time;
      msg->text = PushCopy(arena, ConsumeString(file, msg_header->length));
      msg->control_point_count = msg_header->control_point_count;
      msg->control_points = ConsumeAndCopyArray(arena, file, North_HermiteControlPoint, msg->control_point_count);
   }
}

void LoadRecording(LoadedRobotRecording *state, string file_name) {
   MemoryArena *arena = &state->arena;
   
   Reset(arena);
   buffer file = ReadEntireFile(Concat(file_name, Literal(".ncrr")));
   state->loaded = (file.data != NULL);
   if(file.data != NULL) {
      FileHeader *file_numbers = ConsumeStruct(&file, FileHeader);

      if((file_numbers->magic_number == ROBOT_RECORDING_MAGIC_NUMBER) && 
         (file_numbers->version_number == ROBOT_RECORDING_CURR_VERSION))
      {
         RobotRecording_FileHeader *header = ConsumeStruct(&file, RobotRecording_FileHeader);
         
         state->timestamp = header->timestamp;
         state->robot_name = PushCopy(arena,
            ConsumeString(&file, header->robot_name_length));
         state->group_count = header->group_count;
         state->groups = PushArray(arena, LoadedRecordingGroup, header->group_count);

         RobotRecording_RobotStateSample *robot_samples =
            ConsumeArray(&file, RobotRecording_RobotStateSample, header->robot_state_sample_count);
         
         if(header->robot_state_sample_count > 0) {
            state->robot_sample_count = header->robot_state_sample_count;
            state->robot_samples = (RobotRecording_RobotStateSample *) PushCopy(arena, robot_samples,
                                             header->robot_state_sample_count * sizeof(RobotRecording_RobotStateSample));
         
            state->min_time = F32_MAX;
            state->max_time = -F32_MAX;
            
            for(u32 i = 0; i < header->robot_state_sample_count; i++) {
               RobotRecording_RobotStateSample *sample = robot_samples + i;
               state->min_time = Min(state->min_time, sample->time);
               state->max_time = Max(state->max_time, sample->time);
            }

            state->curr_time = state->min_time;
         }

         LoadRecordingGroup(arena, &file, &state->default_group);
         for(u32 i = 0; i < state->group_count; i++) {
            LoadRecordingGroup(arena, &file, state->groups + i);
         }
      }
   }
}

//RECORDING-------------------------------------
struct RobotStateSampleBlock {
   RobotStateSampleBlock *next;
   RobotRecording_RobotStateSample samples[1024];
   u32 count;
};

struct RecorderDiagnosticSampleBlock {
   RecorderDiagnosticSampleBlock *next;
   RobotRecording_DiagnosticSample samples[128];
   u32 count;
};

struct RecorderDiagnostic {
   RecorderDiagnostic *next_in_list;
   RecorderDiagnostic *next_in_hash;
   
   string name;
   North_Unit::type unit;

   u32 diagnostic_sample_count;
   RecorderDiagnosticSampleBlock first_sample_block;
   RecorderDiagnosticSampleBlock *curr_sample_block;
};

enum MessagelikeType {
   Messagelike_Message,
   Messagelike_Marker,
   Messagelike_Path
};

struct RecorderMessagelike {
   RecorderMessagelike *next;
   bool updated;

   f32 begin_time;
   f32 end_time;
   string text;

   MessagelikeType type;

   union {
      struct {
         North_MessageType::type type;
      } message;

      struct {
         v2 pos;
      } marker;

      struct {
         u32 control_point_count;
         North_HermiteControlPoint *control_points;
      } path;
   };
};

struct RecorderCompleteList {
   RecorderMessagelike *active;
   
   u32 complete_count;
   RecorderMessagelike *complete;
};

struct RobotRecorder;
struct RecorderGroup {
   RobotRecorder *state;
   RecorderGroup *next_in_list;
   RecorderGroup *next_in_hash;

   string name;

   u32 diagnostic_count;
   RecorderDiagnostic *first_diagnostic;
   RecorderDiagnostic *diagnostic_hash[64];

   RecorderCompleteList messages;
   RecorderCompleteList markers;
   RecorderCompleteList paths;
};

struct RobotRecorder {
   MemoryArena arena;
   bool recording;
   f32 latest_time;

   u32 sample_count;
   RobotStateSampleBlock first_sample_block;
   RobotStateSampleBlock *curr_sample_block;

   u32 group_count;
   RecorderGroup default_group;
   RecorderGroup *first_group;
   RecorderGroup *group_hash[64];
};

//TODO: abstract out this hash & list stuff so theres less copy-pasta
//TODO: same for the block list stuff
//spent a few hours trying to come up with a good solution, we'll come back to this later 

RecorderGroup *GetOrCreateRecorderGroup(RobotRecorder *state, string name) {
   if(name.length == 0)
      return &state->default_group;
   
   MemoryArena *arena = &state->arena;

   u32 hash = Hash(name) % ArraySize(state->group_hash);
   RecorderGroup *result = NULL;
   for(RecorderGroup *curr = state->group_hash[hash]; curr; curr = curr->next_in_hash) {
      if(curr->name == name) {
         result = curr;
      }
   }

   if(result == NULL) {
      auto *new_group = PushStruct(arena, RecorderGroup);
      new_group->name = PushCopy(arena, name);
      new_group->state = state;

      new_group->next_in_hash = state->group_hash[hash];
      new_group->next_in_list = state->first_group;

      state->group_count++;
      state->group_hash[hash] = new_group;
      state->first_group = new_group;
      result = new_group;
   }

   //idea
   /*
   GetOrCreate(RecorderSubsystem, name, new_subsystem, {
      new_subsystem->name = PushCopy(arena, name);
   })
   */

   return result;
}

void AddDiagnosticSample(RecorderGroup *group, 
                         string name, North_Unit::type unit,
                         f32 value, f32 time) 
{
   MemoryArena *arena = &group->state->arena;
   
   u32 hash = Hash(name) % ArraySize(group->diagnostic_hash);
   RecorderDiagnostic *line = NULL;
   for(RecorderDiagnostic *curr = group->diagnostic_hash[hash]; curr; curr = curr->next_in_hash) {
      if(curr->name == name) {
         line = curr;
      }
   }

   if(line == NULL) {
      RecorderDiagnostic *new_line = PushStruct(arena, RecorderDiagnostic);
      new_line->name = PushCopy(arena, name);
      new_line->unit = unit;

      new_line->next_in_hash = group->diagnostic_hash[hash];
      new_line->next_in_list = group->first_diagnostic;
      new_line->curr_sample_block = &new_line->first_sample_block;

      group->diagnostic_hash[hash] = new_line;
      group->first_diagnostic = new_line;
      group->diagnostic_count++;
      line = new_line;
   }
   
   if(line->curr_sample_block->count >= ArraySize(line->curr_sample_block->samples)) {
      RecorderDiagnosticSampleBlock *new_sample_block = PushStruct(arena, RecorderDiagnosticSampleBlock);

      line->curr_sample_block->next = new_sample_block;
      line->curr_sample_block = new_sample_block;
   }
   line->curr_sample_block->samples[line->curr_sample_block->count++] = { value, time };
   line->diagnostic_sample_count++;
}

bool Equal(RecorderMessagelike *msg, RecorderMessagelike *data) {
   if(msg->text != data->text)
      return false;
   
   switch(msg->type) {
      case Messagelike_Message: {
         return (msg->message.type == data->message.type);   
      } break;
      
      case Messagelike_Marker: {
         //TODO: this form of msg->pos == pos is both slow and ugly, fix
         return (Length(msg->marker.pos - data->marker.pos) < 0.0001);   
      } break;

      case Messagelike_Path: {
         if(msg->path.control_point_count == data->path.control_point_count) {
            for(u32 i = 0; i < msg->path.control_point_count; i++) {
               North_HermiteControlPoint a = msg->path.control_points[i];
               North_HermiteControlPoint b = data->path.control_points[i];

               if((Length(a.pos - b.pos) > 0.0001) ||
                  (Length(a.tangent - b.tangent) > 0.0001))
               {
                  return false;
               }
            }

            return true;
         } else {
            return false;
         }
      } break;
   }
   
   Assert(false);
   return false;
}

void AddOrUpdate(MemoryArena *arena, RecorderCompleteList *list,
                 f32 time, MessagelikeType type,
                 RecorderMessagelike *data)
{
   for(RecorderMessagelike *curr = list->active; curr; curr = curr->next) {
      if(Equal(curr, data)) {
         curr->updated = true;
         return;
      }
   }

   RecorderMessagelike *new_message = PushStruct(arena, RecorderMessagelike);
   new_message->text = PushCopy(arena, data->text);
   new_message->begin_time = time;
   new_message->type = type;
   
   switch(type) {
      case Messagelike_Message: {
         new_message->message.type = data->message.type;
      } break;
      
      case Messagelike_Marker: {
         new_message->marker.pos = data->marker.pos;
      } break;

      case Messagelike_Path: {
         new_message->path.control_point_count = data->path.control_point_count;
         new_message->path.control_points = PushArrayCopy(arena, North_HermiteControlPoint, data->path.control_points, data->path.control_point_count);
      } break;
   }

   new_message->next = list->active;
   new_message->updated = true;
   list->active = new_message;
}

void AddOrUpdateMessage(RecorderGroup *group, f32 time, string text, North_MessageType::type type) {
   RecorderMessagelike data = {};
   data.text = text;
   data.message.type = type;
   
   AddOrUpdate(&group->state->arena, &group->messages, 
               time, Messagelike_Message, &data);
} 

void AddOrUpdateMarker(RecorderGroup *group, f32 time, string text, v2 pos) {
   RecorderMessagelike data = {};
   data.text = text;
   data.marker.pos = pos;
   
   AddOrUpdate(&group->state->arena, &group->markers, 
               time, Messagelike_Marker, &data);
} 

void AddOrUpdatePath(RecorderGroup *group, f32 time, string text, 
                     u32 control_point_count,
                     North_HermiteControlPoint *control_points)
{
   RecorderMessagelike data = {};
   data.text = text;
   data.path.control_point_count = control_point_count;
   data.path.control_points = control_points;
   
   AddOrUpdate(&group->state->arena, &group->paths, 
               time, Messagelike_Path, &data);
}

void BeginUpdates(RecorderCompleteList *list) {
   for(RecorderMessagelike *curr = list->active; 
       curr; curr = curr->next) 
   {
      curr->updated = false;
   }
}

//TODO: step through this in the debugger, not 100% sure its correct
void CheckForComplete(RecorderCompleteList *list, f32 time) {
   RecorderMessagelike *next = list->active;
   RecorderMessagelike *prev = NULL;

   while(next) {
      RecorderMessagelike *curr = next;
      next = curr->next;

      if(!curr->updated) {
         curr->end_time = time;
         
         list->complete_count++;
         curr->next = list->complete;
         list->complete = curr;

         if(prev != NULL)
            prev->next = next;
      } else {
         prev = curr;
      }
   }
}

void CompleteAll(RecorderCompleteList *list, f32 time) {
   BeginUpdates(list);
   CheckForComplete(list, time);
}

void BeginRecording(RobotRecorder *state /*, string name*/) {
   Assert(!state->recording);
   state->recording = true;

   Reset(&state->arena);
   state->first_group = NULL;
   state->group_count = 0;

   _Zero((u8 *) state->group_hash, sizeof(state->group_hash));
   ZeroStruct(&state->default_group);
   state->default_group.state = state;

   ZeroStruct(&state->first_sample_block);
   state->curr_sample_block = &state->first_sample_block;
   state->sample_count = 0;
}

//WRITING-NCRR-FILES----------------------------------
void WriteGroup(buffer *file, RecorderGroup *group) {
   CompleteAll(&group->messages, group->state->latest_time);
   CompleteAll(&group->markers, group->state->latest_time);
   CompleteAll(&group->paths, group->state->latest_time);
   
   RobotRecording_Group group_header = {};
   group_header.name_length = group->name.length;
   group_header.diagnostic_count = group->diagnostic_count;
   group_header.message_count = group->messages.complete_count;
   group_header.marker_count = group->markers.complete_count;
   group_header.path_count = group->paths.complete_count;
   WriteStruct(file, &group_header);
   WriteString(file, group->name);

   for(RecorderDiagnostic *diag = group->first_diagnostic;
       diag; diag = diag->next_in_list)
   {
      RobotRecording_Diagnostic diag_header = {};
      diag_header.name_length = diag->name.length;
      diag_header.unit = (u8) diag->unit;
      diag_header.sample_count = diag->diagnostic_sample_count;
      WriteStruct(file, &diag_header);
      WriteString(file, diag->name);

      for(RecorderDiagnosticSampleBlock *block = &diag->first_sample_block;
          block; block = block->next)
      {
         WriteArray(file, block->samples, block->count);
      }
   }

   for(RecorderMessagelike *msg = group->messages.complete;
       msg; msg = msg->next)
   {
      Assert(msg->type == Messagelike_Message);

      RobotRecording_Message message = {};
      message.length = (u16) msg->text.length;
      message.type = (u8) msg->message.type;
      message.begin_time = msg->begin_time;
      message.end_time = msg->end_time;
      WriteStruct(file, &message);
      WriteString(file, msg->text);
   }

   for(RecorderMessagelike *msg = group->markers.complete;
       msg; msg = msg->next)
   {
      Assert(msg->type == Messagelike_Marker);

      RobotRecording_Marker marker = {};
      marker.length = (u16) msg->text.length;
      marker.pos = msg->marker.pos;
      marker.begin_time = msg->begin_time;
      marker.end_time = msg->end_time;
      WriteStruct(file, &marker);
      WriteString(file, msg->text);
   }
   
   for(RecorderMessagelike *msg = group->paths.complete;
       msg; msg = msg->next)
   {
      Assert(msg->type == Messagelike_Path);

      RobotRecording_Path path = {};
      path.length = (u16) msg->text.length;
      path.control_point_count = (u8) msg->path.control_point_count;
      path.begin_time = msg->begin_time;
      path.end_time = msg->end_time;
      WriteStruct(file, &path);
      WriteString(file, msg->text);
      WriteArray(file, msg->path.control_points, msg->path.control_point_count);
   }
}

void AppendChunk(RobotRecorder *state) {
   buffer chunk = PushTempBuffer(Megabyte(5));
   //TODO
}

void WriteHeader(RobotRecorder *state) {
   //TODO
}

void EndRecording(RobotRecorder *state, string recording_name) {
   Assert(state->recording);
   state->recording = false;
   
   buffer file = PushTempBuffer(Megabyte(5));

   FileHeader numbers = header(ROBOT_RECORDING_MAGIC_NUMBER, ROBOT_RECORDING_CURR_VERSION);
   WriteStruct(&file, &numbers);
   
   RobotRecording_FileHeader header = {};
   header.timestamp = 0; //TODO: actually get the timestamp
   header.robot_name_length = recording_name.length; //TODO: actually get the robot name
   header.group_count = state->group_count;
   header.robot_state_sample_count = state->sample_count;
   WriteStruct(&file, &header);
   WriteString(&file, recording_name);

   {
      RobotRecording_RobotStateSample *samples = PushTempArray(RobotRecording_RobotStateSample, state->sample_count);
      u32 sample_i = 0;
      for(RobotStateSampleBlock *curr = &state->first_sample_block;
         curr; curr = curr->next) {
         for(u32 i = 0; i < curr->count; i++) {
            samples[sample_i++] = curr->samples[i];
         }
      }
      WriteArray(&file, samples, state->sample_count);
   }

   WriteGroup(&file, &state->default_group);
   for(RecorderGroup *group = state->first_group;
       group; group = group->next_in_list)
   {
      WriteGroup(&file, group);
   }

   WriteEntireFile(Concat(recording_name, Literal(".ncrr")), file);
}

//PARSING-STATE-PACKETS------------------------------
void StatePacket_ParseGroup(f32 time, buffer *packet, RobotRecorder *state) {
   State_Group *header = ConsumeStruct(packet, State_Group);
   string group_name = ConsumeString(packet, header->name_length);
   
   RecorderGroup *group = GetOrCreateRecorderGroup(state, group_name);

   for(u32 i = 0; i < header->diagnostic_count; i++) {
      State_Diagnostic *diag_header = ConsumeStruct(packet, State_Diagnostic);
      string diag_name = ConsumeString(packet, diag_header->name_length);


      AddDiagnosticSample(group, diag_name, 
                          (North_Unit::type) diag_header->unit,
                          diag_header->value, time); 
   }

   BeginUpdates(&group->messages);
   for(u32 i = 0; i < header->message_count; i++) {
      State_Message *packet_message = ConsumeStruct(packet, State_Message);
      string text = ConsumeString(packet, packet_message->length);
      AddOrUpdateMessage(group, time, text,
                         (North_MessageType::type) packet_message->type);
   }
   CheckForComplete(&group->messages, time);

   BeginUpdates(&group->markers);
   for(u32 i = 0; i < header->marker_count; i++) {
      State_Marker *packet_marker = ConsumeStruct(packet, State_Marker);
      string text = ConsumeString(packet, packet_marker->length);
      AddOrUpdateMarker(group, time, text, packet_marker->pos);
   }
   CheckForComplete(&group->markers, time);

   BeginUpdates(&group->paths);
   for(u32 i = 0; i < header->path_count; i++) {
      State_Path *packet_path = ConsumeStruct(packet, State_Path);
      string text = ConsumeString(packet, packet_path->length);
      North_HermiteControlPoint *control_points = ConsumeArray(packet, North_HermiteControlPoint, packet_path->control_point_count);
      AddOrUpdatePath(group, time, text, 
                      packet_path->control_point_count,
                      control_points);
   }
   CheckForComplete(&group->paths, time);
}

void RecieveStatePacket(RobotRecorder *state, buffer packet) {
   if(!state->recording)
      return;

   MemoryArena *arena = &state->arena;

   //TODO: if we're run out of space in the arena, 
   //      write a chunk & reset the arena before parsing the packet 

   State_PacketHeader *header = ConsumeStruct(&packet, State_PacketHeader);
   f32 time = header->time;
   state->latest_time = time;
   
   if(state->curr_sample_block->count >= ArraySize(state->curr_sample_block->samples)) {
      RobotStateSampleBlock *new_sample_block = PushStruct(arena, RobotStateSampleBlock);

      state->curr_sample_block->next = new_sample_block;
      state->curr_sample_block = new_sample_block;
   }
   state->curr_sample_block->samples[state->curr_sample_block->count++] = { header->pos, header->angle, time };
   state->sample_count++;

   StatePacket_ParseGroup(time, &packet, state);
   for(u32 i = 0; i < header->group_count; i++) {
      StatePacket_ParseGroup(time, &packet, state);
   }
}