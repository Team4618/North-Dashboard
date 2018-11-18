struct SubsystemRecording {
   string name;
   MultiLineGraphData graph;
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

   u32 subsystem_count;
   SubsystemRecording *subsystems;

   u32 message_count;
   MessageRecording *messages;

   u32 marker_count;
   MarkerRecording *markers;
};

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

         SubsystemRecording *subsystems = PushArray(arena, SubsystemRecording, header->subsystem_count);
         state->subsystem_count = header->subsystem_count;
         state->subsystems = subsystems;

         for(u32 i = 0; i < header->subsystem_count; i++) {
            SubsystemRecording *subsystem_graph = subsystems + i;
            RobotRecording_SubsystemDiagnostics *subsystem = 
               ConsumeStruct(&file, RobotRecording_SubsystemDiagnostics);

            subsystem_graph->name = PushCopy(arena, 
                                             ConsumeString(&file, subsystem->name_length));
            
            //TODO: way to specify this size
            MemoryArena graph_arena = PlatformAllocArena(Megabyte(2)); 

            subsystem_graph->graph = NewMultiLineGraph(graph_arena);
            SetDiagnosticsGraphUnits(&subsystem_graph->graph);

            for(u32 j = 0; j < subsystem->diagnostic_count; j++) {
               RobotRecording_Diagnostic *line = ConsumeStruct(&file, RobotRecording_Diagnostic);
               string line_name = PushCopy(arena, ConsumeString(&file, line->name_length));

               for(u32 k = 0; k < line->sample_count; k++) {
                  RobotRecording_DiagnosticSample *sample = ConsumeStruct(&file, RobotRecording_DiagnosticSample);
                  AddEntry(&subsystem_graph->graph, line_name, sample->value, sample->time, line->unit);
               }
            }
         }

         MessageRecording *messages = PushArray(arena, MessageRecording, header->message_count);
         state->message_count = header->message_count;
         state->messages = messages;

         for(u32 i = 0; i < header->message_count; i++) {
            MessageRecording *recording = messages + i;
            RobotRecording_Message *message = ConsumeStruct(&file, RobotRecording_Message);
            
            recording->text = PushCopy(arena, ConsumeString(&file, message->length));
            recording->type = (North_MessageType::type) message->type;
            recording->begin_time = message->begin_time;
            recording->end_time = message->end_time;
         }

         MarkerRecording *markers = PushArray(arena, MarkerRecording, header->marker_count);
         state->marker_count = header->marker_count;
         state->markers = markers;

         for(u32 i = 0; i < header->marker_count; i++) {
            MarkerRecording *recording = markers + i;
            RobotRecording_Marker *marker = ConsumeStruct(&file, RobotRecording_Marker);
            
            recording->text = PushCopy(arena, ConsumeString(&file, marker->length));
            recording->pos = marker->pos;
            recording->begin_time = marker->begin_time;
            recording->end_time = marker->end_time;
         }
      }

      FreeEntireFile(&file);
   }
}

struct RobotStateSampleBlock {
   RobotStateSampleBlock *next;
   RobotRecording_RobotStateSample samples[1024];
   u32 count;
};

struct RecorderSubsystemDiagnosticSampleBlock {
   RecorderSubsystemDiagnosticSampleBlock *next;
   RobotRecording_DiagnosticSample samples[128];
   u32 count;
};

struct RecorderSubsystemDiagnostic {
   RecorderSubsystemDiagnostic *next_in_list;
   RecorderSubsystemDiagnostic *next_in_hash;
   
   string name;
   North_Unit::type unit;

   u32 diagnostic_sample_count;
   RecorderSubsystemDiagnosticSampleBlock first_sample_block;
   RecorderSubsystemDiagnosticSampleBlock *curr_sample_block;
};

struct RecorderSubsystem {
   RecorderSubsystem *next_in_list;
   RecorderSubsystem *next_in_hash;

   string name;

   u32 diagnostic_count;
   RecorderSubsystemDiagnostic *first_diagnostic;
   RecorderSubsystemDiagnostic *diagnostic_hash[64];
};

struct RecorderMessageOrMarker {
   RecorderMessageOrMarker *next;
   bool updated;

   f32 begin_time;
   f32 end_time;
   string text;

   bool is_message; //NOTE: if true, use type, else use pos
   v2 pos;
   North_MessageType::type type;
};

struct RecorderCompleteList {
   RecorderMessageOrMarker *active;
   
   u32 complete_count;
   RecorderMessageOrMarker *complete;
};

struct RobotRecorder {
   MemoryArena arena;
   bool recording;

   u32 sample_count;
   RobotStateSampleBlock first_sample_block;
   RobotStateSampleBlock *curr_sample_block;

   u32 subsystem_count;
   RecorderSubsystem *first_subsystem;
   RecorderSubsystem *subsystem_hash[64];

   RecorderCompleteList messages;
   RecorderCompleteList markers;
};

//TODO: abstract out this hash & list stuff so theres less copy-pasta
//TODO: same for the block list stuff
//spent a few hours trying to come up with a good solution, we'll come back to this later 

RecorderSubsystem *GetOrCreateRecorderSubsystem(RobotRecorder *state, string name) {
   MemoryArena *arena = &state->arena;

   u32 hash = Hash(name) % ArraySize(state->subsystem_hash);
   RecorderSubsystem *result = NULL;
   for(RecorderSubsystem *curr = state->subsystem_hash[hash]; curr; curr = curr->next_in_hash) {
      if(curr->name == name) {
         result = curr;
      }
   }

   if(result == NULL) {
      auto *new_subsystem = PushStruct(arena, RecorderSubsystem);
      new_subsystem->name = PushCopy(arena, name);

      new_subsystem->next_in_hash = state->subsystem_hash[hash];
      new_subsystem->next_in_list = state->first_subsystem;

      state->subsystem_hash[hash] = new_subsystem;
      state->first_subsystem = new_subsystem;
      state->subsystem_count++;
      result = new_subsystem;
   }

   //idea
   /*
   GetOrCreate(RecorderSubsystem, name, new_subsystem, {
      new_subsystem->name = PushCopy(arena, name);
   })
   */

   return result;
}

void AddDiagnosticSample(RobotRecorder *state, RecorderSubsystem *subsystem, string name, North_Unit::type unit,
                         f32 value, f32 time) 
{
   MemoryArena *arena = &state->arena;
   
   u32 hash = Hash(name) % ArraySize(subsystem->diagnostic_hash);
   RecorderSubsystemDiagnostic *line = NULL;
   for(RecorderSubsystemDiagnostic *curr = subsystem->diagnostic_hash[hash]; curr; curr = curr->next_in_hash) {
      if(curr->name == name) {
         line = curr;
      }
   }

   if(line == NULL) {
      RecorderSubsystemDiagnostic *new_line = PushStruct(arena, RecorderSubsystemDiagnostic);
      new_line->name = PushCopy(arena, name);
      new_line->unit = unit;

      new_line->next_in_hash = subsystem->diagnostic_hash[hash];
      new_line->next_in_list = subsystem->first_diagnostic;
      new_line->curr_sample_block = &new_line->first_sample_block;

      subsystem->diagnostic_hash[hash] = new_line;
      subsystem->first_diagnostic = new_line;
      subsystem->diagnostic_count++;
      line = new_line;
   }
   
   RecorderSubsystemDiagnosticSampleBlock *curr_block = line->curr_sample_block;
   if(curr_block->count >= ArraySize(curr_block->samples)) {
      RecorderSubsystemDiagnosticSampleBlock *new_sample_block = PushStruct(arena, RecorderSubsystemDiagnosticSampleBlock);

      line->curr_sample_block->next = new_sample_block;
      line->curr_sample_block = new_sample_block;
   }
   curr_block->samples[curr_block->count++] = { value, time };
   line->diagnostic_sample_count++;
}

bool Equal(RecorderMessageOrMarker *msg, string text, v2 pos, North_MessageType::type type) {
   if(msg->is_message) {
      return (msg->text == text) && (msg->type == type);
   } else {
      //TODO: this form of msg->pos == pos is both slow and ugly, fix
      return (msg->text == text) && (Length(msg->pos - pos) < 0.0001);
   }
}

void AddOrUpdate(MemoryArena *arena, RecorderCompleteList *list,
                 f32 time, string text,
                 v2 pos, North_MessageType::type type, bool is_message)
{
   for(RecorderMessageOrMarker *curr = list->active; curr; curr = curr->next) {
      if(Equal(curr, text, pos, type)) {
         curr->updated = true;
         break;
      }
   }

   RecorderMessageOrMarker *new_message = PushStruct(arena, RecorderMessageOrMarker);
   new_message->text = PushCopy(arena, text);
   new_message->begin_time = time;
   
   new_message->is_message = is_message;
   new_message->pos = pos;
   new_message->type = type;

   new_message->next = list->active;
   new_message->updated = true;
   list->active = new_message;
}

void AddOrUpdateMessage(RobotRecorder *state, f32 time, string text, North_MessageType::type type) {
   AddOrUpdate(&state->arena, &state->messages, time, text, V2(0, 0), type, true);
} 

void AddOrUpdateMarker(RobotRecorder *state, f32 time, string text, v2 pos) {
   AddOrUpdate(&state->arena, &state->markers, time, text, pos, North_MessageType::Message, false);
} 

void CheckForComplete(RecorderCompleteList *list, f32 time) {
   RecorderMessageOrMarker *next = list->active;
   while(next) {
      RecorderMessageOrMarker *curr = next;
      next = curr->next;

      if(!curr->updated) {
         curr->end_time = time;
         
         list->complete_count++;
         curr->next = list->complete;
         list->complete = curr;
      }
   }
}

void BeginRecording(RobotRecorder *state) {
   Assert(!state->recording);
   state->recording = true;

   Reset(&state->arena);
   ZeroStruct(&state->first_sample_block);
   state->curr_sample_block = &state->first_sample_block;
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
   header.subsystem_count = state->subsystem_count;
   header.robot_state_sample_count = state->sample_count;
   header.message_count = state->messages.complete_count;
   header.marker_count = state->markers.complete_count;
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

   for(RecorderSubsystem *subsystem = state->first_subsystem;
       subsystem; subsystem = subsystem->next_in_list) 
   {
      RobotRecording_SubsystemDiagnostics diag = {};
      diag.name_length = subsystem->name.length;
      diag.diagnostic_count = subsystem->diagnostic_count;
      WriteStruct(&file, &diag);
      WriteString(&file, subsystem->name);

      for(RecorderSubsystemDiagnostic *line = subsystem->first_diagnostic;
          line; line = line->next_in_list)
      {
         RobotRecording_Diagnostic diag_line = {};
         diag_line.name_length = line->name.length;
         diag_line.unit = (u8) line->unit;
         diag_line.sample_count = line->diagnostic_sample_count;
         WriteStruct(&file, &diag_line);
         WriteString(&file, line->name);

         RobotRecording_DiagnosticSample *samples = PushTempArray(RobotRecording_DiagnosticSample, line->diagnostic_sample_count);
         u32 sample_i = 0;
         for(RecorderSubsystemDiagnosticSampleBlock *sample_block = line->curr_sample_block; 
             sample_block; sample_block = sample_block->next)
         {
            for(u32 i = 0; i < sample_block->count; i++) {
               samples[sample_i++] = sample_block->samples[i];
            }
         }
         WriteArray(&file, samples, line->diagnostic_sample_count);
      }
   }

   for(RecorderMessageOrMarker *msg = state->messages.complete;
       msg; msg = msg->next)
   {
      RobotRecording_Message message = {};
      message.length = (u16) msg->text.length;
      message.type = (u8) msg->type;
      message.begin_time = msg->begin_time;
      message.end_time = msg->end_time;
      WriteStruct(&file, &message);
      WriteString(&file, msg->text);
   }

   for(RecorderMessageOrMarker *msg = state->markers.complete;
       msg; msg = msg->next)
   {
      RobotRecording_Marker marker = {};
      marker.length = (u16) msg->text.length;
      marker.pos = msg->pos;
      marker.begin_time = msg->begin_time;
      marker.end_time = msg->end_time;
      WriteStruct(&file, &marker);
      WriteString(&file, msg->text);
   }

   WriteEntireFile(Concat(recording_name, Literal(".ncrr")), file);
}

//TODO: if we run out of space in the arena, dump the current recording to a file, clear & continue recording
void RecieveStatePacket(RobotRecorder *state, buffer packet) {
   if(!state->recording)
      return;

   MemoryArena *arena = &state->arena;

   State_PacketHeader *header = ConsumeStruct(&packet, State_PacketHeader);
   f32 time = header->time;
   
   if(state->curr_sample_block->count >= ArraySize(state->curr_sample_block->samples)) {
      RobotStateSampleBlock *new_sample_block = PushStruct(arena, RobotStateSampleBlock);

      state->curr_sample_block->next = new_sample_block;
      state->curr_sample_block = new_sample_block;
   }
   state->curr_sample_block->samples[state->curr_sample_block->count++] = { header->pos, header->angle, time };
   state->sample_count++;

   for(u32 i = 0; i < header->subsystem_count; i++) {
      State_SubsystemDiagnostics *subsystem_diag = ConsumeStruct(&packet, State_SubsystemDiagnostics);
      string subsystem_name = ConsumeString(&packet, subsystem_diag->name_length);
      
      RecorderSubsystem *subsystem = GetOrCreateRecorderSubsystem(state, subsystem_name);

      for(u32 j = 0; j < subsystem_diag->diagnostic_count; j++) {
         State_Diagnostic *diag_sample = ConsumeStruct(&packet, State_Diagnostic);
         string name = ConsumeString(&packet, diag_sample->name_length);
         
         AddDiagnosticSample(state, subsystem, name, 
                             (North_Unit::type) diag_sample->unit,
                             diag_sample->value, time);
      }
   }

   for(u32 i = 0; i < header->message_count; i++) {
      State_Message *packet_message = ConsumeStruct(&packet, State_Message);
      string text = ConsumeString(&packet, packet_message->length);
      AddOrUpdateMessage(state, time, text,
                         (North_MessageType::type) packet_message->type);
   }
   CheckForComplete(&state->messages, time);

   for(u32 i = 0; i < header->marker_count; i++) {
      State_Marker *packet_marker = ConsumeStruct(&packet, State_Marker);
      string text = ConsumeString(&packet, packet_marker->length);
      AddOrUpdateMarker(state, time, text, packet_marker->pos);
   }
   CheckForComplete(&state->markers, time);
}