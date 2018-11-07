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
            u32 graph_arena_size = Megabyte(2);
            MemoryArena graph_arena = NewMemoryArena(PushSize(arena, graph_arena_size), graph_arena_size); 

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
   RobotRecording_RobotStateSample samples[256];
   u32 count;
};

struct RecorderMessage {
   RecorderMessage *next;

   North_MessageType::type type;
   string text;
   f32 begin_time;
   f32 end_time;
};

struct RecorderMarker {
   RecorderMarker *next;

   v2 pos;
   string text;
   f32 begin_time;
   f32 end_time;
};

struct RobotRecorder {
   MemoryArena arena;
   bool recording;

   RobotStateSampleBlock first_sample_block;
   RobotStateSampleBlock *curr_sample_block;

   RecorderMessage *active_messages;
   RecorderMessage *past_messages;

   RecorderMarker *active_markers;
   RecorderMarker *past_markers;
};

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
   
   //TODO: write out file as recording_name + ".ncrr"
}

void RecieveStatePacket(RobotRecorder *state, buffer packet) {
   if(!state->recording)
      return;

   MemoryArena *arena = &state->arena;

   //TODO: parse packet
   State_PacketHeader *header = ConsumeStruct(&packet, State_PacketHeader);
   f32 time = header->time;
   
   if(state->curr_sample_block->count >= ArraySize(state->curr_sample_block->samples)) {
      RobotStateSampleBlock *new_sample_block = PushStruct(arena, RobotStateSampleBlock);

      state->curr_sample_block->next = new_sample_block;
      state->curr_sample_block = new_sample_block;
   }
   state->curr_sample_block->samples[state->curr_sample_block->count++] = { header->pos, header->angle, time };

   //TODO: data backing for subsystem diagnostics
   for(u32 i = 0; i < header->subsystem_count; i++) {
      State_SubsystemDiagnostics *subsystem_diag = ConsumeStruct(&packet, State_SubsystemDiagnostics);
      string subsystem_name = ConsumeString(&packet, subsystem_diag->name_length);
      
      for(u32 j = 0; j < subsystem_diag->diagnostic_count; j++) {
         State_Diagnostic *diag_sample = ConsumeStruct(&packet, State_Diagnostic);
         string name = ConsumeString(&packet, diag_sample->name_length);
      }
   }

   //TODO: data backing for messages & markers
   for(u32 i = 0; i < header->message_count; i++) {
      State_Message *packet_message = ConsumeStruct(&packet, State_Message);
      string text = ConsumeString(&packet, packet_message->length);
   }

   for(u32 i = 0; i < header->marker_count; i++) {
      State_Marker *packet_marker = ConsumeStruct(&packet, State_Marker);
      string text = ConsumeString(&packet, packet_marker->length);
   }
}