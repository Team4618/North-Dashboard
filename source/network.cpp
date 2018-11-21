void PacketHandler_Welcome(buffer *packet_in, DashboardState *state) {
   buffer packet = *packet_in;
   MemoryArena *arena = &state->connected.arena;
   
   Reset(arena);
   Welcome_PacketHeader *header = ConsumeStruct(&packet, Welcome_PacketHeader);
   ConsumeString(&packet, header->robot_name_length);

   state->connected.subsystems = PushArray(arena, ConnectedSubsystem, header->subsystem_count);
   state->connected.subsystem_count = header->subsystem_count;

   for(u32 i = 0; i < header->subsystem_count; i++) {
      ConnectedSubsystem *subsystem = state->connected.subsystems + i;
      Welcome_SubsystemDescription *desc = ConsumeStruct(&packet, Welcome_SubsystemDescription);
      
      subsystem->name = PushCopy(arena, ConsumeString(&packet, desc->name_length));
      //TODO: this is really bad
      subsystem->param_arena = PlatformAllocArena(Megabyte(1));
      subsystem->diagnostics_graph = NewMultiLineGraph(PlatformAllocArena(Megabyte(10)));

      for(u32 j = 0; j < desc->command_count; j++) {
         Welcome_SubsystemCommand *command = ConsumeStruct(&packet, Welcome_SubsystemCommand);
         ConsumeString(&packet, command->name_length);
         for(u32 k = 0; k < command->param_count; k++) {
            u8 len = *ConsumeStruct(&packet, u8);
            ConsumeString(&packet, len);
         }
      }
   }

   //TODO: connected_robot codepath (init ConnectedSubsystem s)
   RecieveWelcomePacket(&state->current_profile, *packet_in);
}

void PacketHandler_CurrentParameters(buffer *packet, DashboardState *state) {
   //TODO: connected_robot codepath (update params)
   RecieveCurrentParametersPacket(&state->current_profile, *packet);
}

void PacketHandler_State(buffer *packet, DashboardState *state) {
   //TODO: connected_robot codepath (add to graphs)
   RecieveStatePacket(&state->auto_recorder, *packet);
   RecieveStatePacket(&state->manual_recorder, *packet);
}

void PacketHandler_CurrentAutoPath(buffer *packet, DashboardState *state) {
   //TODO: Dashboard codepath (put on home screen)
}

void HandlePacket(DashboardState *state, PacketType::type type, buffer packet) {
   if(type == PacketType::Welcome) {
      PacketHandler_Welcome(&packet, state);
   } else if(type == PacketType::CurrentParameters) {
      PacketHandler_CurrentParameters(&packet, state);
   } else if(type == PacketType::State) {
      PacketHandler_State(&packet, state);
   } else if(type == PacketType::CurrentAutoPath) {
      PacketHandler_CurrentAutoPath(&packet, state);
   }
}

void HandleDisconnect(DashboardState *state) {
   state->connected.subsystem_count = 0;   
   state->pos_sample_count = 0;
   state->selected_subsystem = NULL;

   if(state->current_profile.state == RobotProfileState::Connected) {
      LoadProfileFile(&state->current_profile, state->current_profile.name);
   }

   if(state->selected_profile == &state->current_profile) {
      state->selected_profile = NULL;
   }

   if(state->auto_recorder.recording)
      EndRecording(&state->auto_recorder, Literal("auto_recording"));

   if(state->manual_recorder.recording)
      EndRecording(&state->manual_recorder, Literal("manual_recording"));
}