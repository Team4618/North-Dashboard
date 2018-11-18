void PacketHandler_Welcome(buffer *packet, DashboardState *state) {
   //TODO: connected_robot codepath (init ConnectedSubsystem s)
   RecieveWelcomePacket(&state->current_profile, *packet);
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
   state->selected_subsystem = NULL;

   if(state->current_profile.state == RobotProfileState::Connected) {
      state->current_profile.state = RobotProfileState::Invalid;
   }

   if(state->auto_recorder.recording)
      EndRecording(&state->auto_recorder, Literal("auto_recording"));

   if(state->manual_recorder.recording)
      EndRecording(&state->manual_recorder, Literal("manual_recording"));
}