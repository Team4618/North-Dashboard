void PacketHandler_Welcome(buffer *packet_in, DashboardState *state) {
   buffer packet = *packet_in;
   MemoryArena *arena = &state->connected.arena;
   
   Reset(arena);
   Welcome_PacketHeader *header = ConsumeStruct(&packet, Welcome_PacketHeader);
   string name = ConsumeString(&packet, header->robot_name_length);

   OutputDebugStringA(ToCString(Concat(Literal("Connected to "), name, Literal("\n"))));


   //TODO: connected_robot codepath 
   RecieveWelcomePacket(&state->profiles.current, *packet_in);
}

void PacketHandler_CurrentParameters(buffer *packet, DashboardState *state) {
   //TODO: connected_robot codepath (update params)
   RecieveCurrentParametersPacket(&state->profiles.current, *packet);
}

void PacketHandler_State(buffer *in_packet, DashboardState *state) {
   //TODO: connected_robot codepath (add to graphs)
   buffer packet = *in_packet;
   

   RecieveStatePacket(&state->auto_recorder, *in_packet);
   RecieveStatePacket(&state->manual_recorder, *in_packet);
}

void HandlePacket(DashboardState *state, PacketType::type type, buffer packet) {
   if(type == PacketType::Welcome) {
      PacketHandler_Welcome(&packet, state);
   } else if(type == PacketType::CurrentParameters) {
      PacketHandler_CurrentParameters(&packet, state);
   } else if(type == PacketType::State) {
      PacketHandler_State(&packet, state);
   }
}

void HandleDisconnect(DashboardState *state) {
   // state->connected.subsystem_count = 0;   
   // state->pos_sample_count = 0;
   // state->selected_subsystem = NULL;

   if(state->profiles.current.state == RobotProfileState::Connected) {
      LoadProfileFile(&state->profiles.current, state->profiles.current.name);
   }

   if(state->profiles.selected == &state->profiles.current) {
      state->profiles.selected = NULL;
   }

   if(state->auto_recorder.recording)
      EndRecording(&state->auto_recorder, Literal("auto_recording"));

   if(state->manual_recorder.recording)
      EndRecording(&state->manual_recorder, Literal("manual_recording"));
}