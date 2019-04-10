void PacketHandler_Welcome(buffer *packet_in, DashboardState *state) {
   RecieveWelcomePacket(&state->profiles.current, *packet_in);
}

void PacketHandler_CurrentParameters(buffer *packet, DashboardState *state) {
   RecieveCurrentParametersPacket(&state->profiles.current, *packet);
}

void PacketHandler_State(buffer *packet, DashboardState *state) {
   ParseStatePacket(&state->connected, *packet);

   RecieveStatePacket(&state->auto_recorder, *packet);
   RecieveStatePacket(&state->manual_recorder, *packet);
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
   ResetConnectedState(&state->connected);
   
   if(state->profiles.current.state == RobotProfileState::Connected) {
      state->profiles.current.state = RobotProfileState::Loaded;
   }

   if(state->profiles.selected == &state->profiles.current) {
      state->profiles.selected = NULL;
   }

   if(state->auto_recorder.recording)
      EndRecording(&state->auto_recorder, Literal("auto_recording"));

   if(state->manual_recorder.recording)
      EndRecording(&state->manual_recorder, Literal("manual_recording"));
}